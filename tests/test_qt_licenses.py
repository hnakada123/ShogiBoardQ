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

    def stage(self, version="6.7.3", library_type="SHARED_LIBRARY"):
        build = self.root / "build"
        build.mkdir(exist_ok=True)
        (build / "qt-build.json").write_text(json.dumps({
            "qt_version": version, "qt_library_type": library_type}))
        (build / "CMakeCache.txt").write_text("")
        qt.stage(SimpleNamespace(notices=self.output, build_dir=build,
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
