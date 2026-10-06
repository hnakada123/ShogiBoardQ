"""Regression tests for matching sources, complete notices and safe extraction."""
import importlib.util
import io
import json
from pathlib import Path
import tarfile
import tempfile
from types import SimpleNamespace
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/qt_licenses.py"
SPEC = importlib.util.spec_from_file_location("qt_licenses", SCRIPT)
qt = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(qt)


class QtLicensesTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.archive = self.root / "qt-everywhere-src-6.7.3.tar.gz"
        self.output = self.root / "notices"
        self.files = {
            "qt/qtbase/.cmake.conf": 'set(QT_REPO_MODULE_VERSION "6.7.3")\n',
            "qt/qtbase/LICENSES/LGPL-3.0-only.txt": "LGPL source text\n",
            "qt/qtbase/LICENSES/GPL-3.0-only.txt": "GPL source text\n",
            "qt/qtbase/src/3rdparty/example/qt_attribution.json": json.dumps({
                "Name": "Example", "LicenseFile": "../terms.txt", "Copyright": "Example authors"}),
            "qt/qtbase/src/3rdparty/terms.txt": "Permission and copyright notice\n",
            "qt/LICENSES/BSD-3-Clause.txt": "Top-level BSD text\n",
            # 配布しないモジュールの文書。参照先の一部は配布するモジュールから共有される。
            "qt/qtwebengine/LICENSES/LGPL-3.0-only.txt": "WebEngine LGPL\n",
            "qt/qtwebengine/src/3rdparty/chromium/LICENSE": "Chromium notice\n",
            "qt/qtwebengine/src/shared/terms.txt": "Shared with a distributed module\n",
            "qt/qtcharts/src/3rdparty/shared/qt_attribution.json": json.dumps({
                "Name": "Shared", "LicenseFiles": ["../../../../qtwebengine/src/shared/terms.txt"]}),
        }

    def prepare(self):
        with tarfile.open(self.archive, "w:gz") as archive:
            for name, text in self.files.items():
                data = text.encode()
                member = tarfile.TarInfo(name)
                member.size = len(data)
                archive.addfile(member, io.BytesIO(data))
        qt.prepare(SimpleNamespace(archive=self.archive, version="6.7.3", sha256=None,
                                   source_url="https://example.invalid/qt.tar.gz",
                                   provenance="Test fixture", output=self.output))

    def stage(self, version="6.7.3", library_type="SHARED_LIBRARY", modules=("qtbase", "qtcharts")):
        build = self.root / "build"
        build.mkdir(exist_ok=True)
        (build / "qt-build.json").write_text(json.dumps({
            "qt_version": version, "qt_library_type": library_type}))
        (build / "CMakeCache.txt").write_text("")
        qt.stage(SimpleNamespace(notices=self.output, build_dir=build, modules=list(modules),
                                 destination=self.root / "deploy/licenses"))

    def test_referenced_notice_preserved_and_sources_identified(self):
        self.prepare()
        self.stage()
        destination = self.root / "deploy/licenses"
        self.assertEqual((destination / "qt/qt/qtbase/src/3rdparty/terms.txt").read_text(),
                         "Permission and copyright notice\n")
        manifest = json.loads((destination / "QT-SOURCE.json").read_text())
        self.assertEqual(manifest["sha256"], qt.digest(self.archive))
        self.assertTrue((destination / "BUILD.json").is_file())

    def test_wrong_build_version_prevents_packaging(self):
        self.prepare()
        with self.assertRaisesRegex(ValueError, "version mismatch"):
            self.stage(version="6.8.0")
        self.assertFalse((self.root / "deploy").exists())

    def test_cached_notices_use_current_localized_guides(self):
        self.prepare()
        (self.output / "SOURCE_CODE.md").write_text("Download Qt sources from Release assets.\n")
        manifest = json.loads((self.output / "QT-SOURCE.json").read_text())
        manifest["release_sources"] = "https://github.com/hnakada123/ShogiBoardQ/releases"
        qt.write_json(self.output / "QT-SOURCE.json", manifest)
        inventory = json.loads((self.output / "FILES.json").read_text())
        # 古いキャッシュには各言語の案内がまだ含まれていない。
        for guide in list(self.output.glob("*.md")):
            if guide.name.startswith(("NOTICE_", "SOURCE_CODE_")):
                guide.unlink()
                del inventory[guide.name]
        for name in ("SOURCE_CODE.md", "QT-SOURCE.json"):
            inventory[name] = qt.digest(self.output / name)
        qt.write_json(self.output / "FILES.json", inventory)

        self.stage()
        destination = self.root / "deploy/licenses"
        for guide in (qt.ROOT / "resources/licenses").glob("*.md"):
            self.assertEqual((destination / guide.name).read_bytes(), guide.read_bytes())
        del manifest["release_sources"]
        self.assertEqual(json.loads((destination / "QT-SOURCE.json").read_text()), manifest)
        for path in (destination / "qt").rglob("*"):
            if path.is_file():
                name = path.relative_to(destination).as_posix()
                self.assertEqual(qt.digest(path), inventory[name])
        self.assertIn("release_sources", json.loads((self.output / "QT-SOURCE.json").read_text()))

    def test_only_distributed_modules_are_staged(self):
        self.prepare()
        destination = self.root / "deploy/licenses"
        # 以前の配置（macOS のバンドルは残る）に含まれていたものは残さない。
        (destination / "qt/qt/qtwebengine").mkdir(parents=True)
        (destination / "qt/qt/qtwebengine/old.txt").write_text("stale")
        (destination / "qt-sdk").mkdir()
        (destination / "FILES.json").write_text("{}")
        self.stage()
        staged = sorted(p.relative_to(destination / "qt").as_posix()
                        for p in (destination / "qt").rglob("*") if p.is_file())
        self.assertEqual(staged, [
            "qt/LICENSES/BSD-3-Clause.txt",
            "qt/qtbase/LICENSES/GPL-3.0-only.txt",
            "qt/qtbase/LICENSES/LGPL-3.0-only.txt",
            "qt/qtbase/src/3rdparty/example/qt_attribution.json",
            "qt/qtbase/src/3rdparty/terms.txt",
            "qt/qtcharts/src/3rdparty/shared/qt_attribution.json",
            # 配布するモジュールが参照する文書は、別モジュールの中でも残す。
            "qt/qtwebengine/src/shared/terms.txt",
        ])
        index = (destination / "QT-NOTICES.md").read_text()
        self.assertIn("qtbase, qtcharts", index)
        self.assertNotIn("chromium", index)
        self.assertEqual(index.count("- ["), len(staged))
        for name in ("NOTICE.md", "SOURCE_CODE.md", "GPL-3.0.txt", "LGPL-3.0.txt",
                     "QT-SOURCE.json", "BUILD.json"):
            self.assertTrue((destination / name).is_file(), name)
        self.assertFalse((destination / "FILES.json").exists())
        self.assertFalse((destination / "qt-sdk").exists())

    def test_missing_module_notices_prevent_packaging(self):
        self.prepare()
        with self.assertRaisesRegex(ValueError, "qtmultimedia"):
            self.stage(modules=("qtbase", "qtmultimedia"))

    def test_changed_license_prevents_packaging(self):
        self.prepare()
        (self.output / "LGPL-3.0.txt").write_text("Incomplete")
        with self.assertRaisesRegex(ValueError, "Notice changed"):
            self.stage()

    def test_missing_reference_prevents_preparation(self):
        del self.files["qt/qtbase/src/3rdparty/terms.txt"]
        with self.assertRaisesRegex(ValueError, "Missing referenced licenses"):
            self.prepare()
        self.assertFalse(self.output.exists())

    def test_wrong_source_version_prevents_preparation(self):
        self.files["qt/qtbase/.cmake.conf"] = 'set(QT_REPO_MODULE_VERSION "6.8.0")'
        with self.assertRaisesRegex(ValueError, "source version"):
            self.prepare()

    def test_unsafe_member_is_not_extracted(self):
        self.files["../outside/LICENSE"] = "unsafe"
        with self.assertRaisesRegex(ValueError, "Unsafe archive"):
            self.prepare()
        self.assertFalse((self.root / "outside").exists())

    def test_static_qt_needs_different_distribution_instructions(self):
        self.prepare()
        with self.assertRaisesRegex(ValueError, "shared Qt"):
            self.stage(library_type="STATIC_LIBRARY")


if __name__ == "__main__":
    unittest.main()
