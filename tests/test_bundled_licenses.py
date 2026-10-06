"""Regression tests for the license notices of non-Qt libraries bundled in the AppImage."""
import importlib.util
from pathlib import Path
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/bundled_licenses.py"
SPEC = importlib.util.spec_from_file_location("bundled_licenses", SCRIPT)
bundled = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(bundled)


class FakeSystem:
    """Package database of a build system; library names map to their packages."""

    def __init__(self, root, packages):
        self.root = root
        self.packages = packages
        self.spdx = root / "spdx"
        self.spdx.mkdir()
        for name in ("LGPL-2.1-or-later", "GPL-2.0-or-later", "LGPL-3.0-or-later"):
            (self.spdx / f"{name}.txt").write_text(f"{name} text\n")

    def host_path(self, path, appdir):
        return Path("/host") / path.relative_to(appdir / "usr")

    def owners(self, paths):
        return {path: package for path in paths
                for package, info in self.packages.items() if path.name in info["files"]}

    def package(self, name):
        info = self.packages[name]
        return {"version": info["version"], "license": info["license"], "url": "https://example.org/" + name,
                "source": "https://gitlab.archlinux.org/archlinux/packaging/packages/" + name}

    def license_files(self, name):
        return [self.root / "pkg" / name / text for text in self.packages[name].get("texts", [])]

    def spdx_text(self, identifier):
        path = self.spdx / f"{identifier}.txt"
        return path if path.is_file() else None


class BundledLicensesTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.appdir = self.root / "AppDir"
        self.destination = self.appdir / "usr/share/licenses/ShogiBoardQ"
        self.packages = {
            "qt6-base": {"version": "6.11.2-3", "license": "LGPL-3.0-only",
                         "files": ["libQt6Core.so.6", "libqxcb.so"]},
            "openssl": {"version": "3.6.0-1", "license": "Apache-2.0",
                        "files": ["libssl.so.3", "libcrypto.so.3"], "texts": ["LICENSE.txt"]},
            "glib2": {"version": "2.88.3-1", "license": "LGPL-2.1-or-later", "files": ["libglib-2.0.so.0"]},
            "libidn2": {"version": "2.3.8-1", "license": "GPL2  LGPL3", "files": ["libidn2.so.0"]},
            "fcitx5-qt": {"version": "5.1.15-1", "license": "LGPL-2.1-or-later  BSD-3-Clause",
                          "files": ["libfcitx5platforminputcontextplugin.so"], "texts": ["BSD-3-Clause.txt"]},
        }
        for name, info in self.packages.items():
            for text in info.get("texts", []):
                path = self.root / "pkg" / name / text
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(f"{name} {text}\n")
        for library in ("libQt6Core.so.6", "libssl.so.3", "libcrypto.so.3", "libglib-2.0.so.0", "libidn2.so.0"):
            self.add("lib", library)
        self.add("plugins/platforms", "libqxcb.so")
        self.add("plugins/platforminputcontexts", "libfcitx5platforminputcontextplugin.so")

    def add(self, directory, name):
        path = self.appdir / "usr" / directory / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"\x7fELF")

    def stage(self):
        return bundled.stage(self.appdir, self.destination, FakeSystem(self.root, self.packages))

    def test_notices_cover_every_bundled_package_except_qt(self):
        # 以前の配置に含まれていたライブラリの文書は残さない。
        stale = self.destination / "third-party/removed/LICENSE"
        stale.parent.mkdir(parents=True)
        stale.write_text("stale")
        self.assertEqual(self.stage(), ["fcitx5-qt", "glib2", "libidn2", "openssl"])
        self.assertFalse(stale.exists())
        index = (self.destination / "THIRD-PARTY-NOTICES.md").read_text()
        self.assertNotIn("qt6-base", index)
        self.assertIn("## openssl 3.6.0-1", index)
        self.assertIn("- Files: `libcrypto.so.3`, `libssl.so.3`", index)
        self.assertIn("`libfcitx5platforminputcontextplugin.so`", index)
        self.assertIn("https://gitlab.archlinux.org/archlinux/packaging/packages/glib2", index)
        # Arch のライセンス欄が SPDX 形式でないものは、上流の表記で補って本文を入れる。
        self.assertIn("- License: LGPL-3.0-or-later OR GPL-2.0-or-later", index)
        third_party = self.destination / "third-party"
        self.assertEqual((third_party / "openssl/LICENSE.txt").read_text(), "openssl LICENSE.txt\n")
        self.assertEqual((third_party / "fcitx5-qt/BSD-3-Clause.txt").read_text(), "fcitx5-qt BSD-3-Clause.txt\n")
        # 共通の本文は spdx/ に1つだけ置き、各パッケージから参照する。
        self.assertEqual(sorted(p.name for p in (third_party / "spdx").iterdir()),
                         ["GPL-2.0-or-later.txt", "LGPL-2.1-or-later.txt", "LGPL-3.0-or-later.txt"])
        self.assertEqual(index.count("(third-party/spdx/LGPL-2.1-or-later.txt)"), 2)
        for link in index.split("](")[1:]:
            target = link.split(")")[0]
            if not target.startswith("https://"):
                self.assertTrue((self.destination / target).is_file(), target)

    def test_package_without_license_text_prevents_packaging(self):
        self.packages["glib2"]["license"] = "LicenseRef-Unknown"
        with self.assertRaisesRegex(ValueError, "No license text for bundled package glib2"):
            self.stage()

    def test_unowned_library_prevents_packaging(self):
        self.add("lib", "libunknown.so.1")
        with self.assertRaisesRegex(ValueError, "No package owns"):
            self.stage()

    def test_license_ids_include_exceptions(self):
        self.assertEqual(bundled.license_ids("GPL-3.0-or-later WITH GCC-exception-3.1  GFDL-1.3-or-later"),
                         ["GPL-3.0-or-later", "GCC-exception-3.1", "GFDL-1.3-or-later"])
        self.assertEqual(bundled.license_ids("(AFL-2.1 OR GPL-2.0-or-later)"), ["AFL-2.1", "GPL-2.0-or-later"])


if __name__ == "__main__":
    unittest.main()
