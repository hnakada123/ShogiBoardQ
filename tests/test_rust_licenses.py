"""Regression tests for the license notices of the Rust crates in the P2P communication process."""
import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/rust_licenses.py"
SPEC = importlib.util.spec_from_file_location("rust_licenses", SCRIPT)
rust = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(rust)


class RenderTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        self.packages = {}

    def tearDown(self):
        self.directory.cleanup()

    def crate(self, name, version, license_expression, texts=None, authors=None):
        directory = self.root / f"{name}-{version}"
        (directory / "src").mkdir(parents=True)
        (directory / "src/LICENSE-nested").write_text("not a top-level license file\n")
        for file_name, text in (texts or {}).items():
            (directory / file_name).write_bytes(text.encode("utf-8"))
        self.packages[(name, version)] = {
            "name": name, "version": version, "license": license_expression, "authors": authors or [],
            "source": "registry+https://github.com/rust-lang/crates.io-index",
            "manifest_path": str(directory / "Cargo.toml")}
        return (name, version)

    def test_lists_crates_and_stores_each_text_once(self):
        apache = "Apache License\nVersion 2.0\n"
        crates = {
            self.crate("beta", "2.0.0", "MIT OR Apache-2.0",
                       {"LICENSE-MIT": "Copyright (c) Beta\n", "LICENSE-APACHE": apache}),
            # 行末の違い（CRLF・行末の空白）だけの同じ本文は1つにまとめる
            self.crate("alpha", "1.0.0", "Apache-2.0", {"LICENSE": "Apache License  \r\nVersion 2.0\r\n\r\n"}),
        }
        document = rust.render(crates, self.packages)
        self.assertIn("## Crates (2)", document)
        self.assertIn("| [alpha](https://crates.io/crates/alpha/1.0.0) | 1.0.0 | Apache-2.0 | T1 |", document)
        self.assertIn("| [beta](https://crates.io/crates/beta/2.0.0) | 2.0.0 | MIT OR Apache-2.0 | T1, T2 |", document)
        self.assertEqual(document.count("Apache License\nVersion 2.0\n"), 1)
        self.assertIn("Used by: alpha 1.0.0 (LICENSE), beta 2.0.0 (LICENSE-APACHE)", document)
        self.assertNotIn("not a top-level license file", document)
        # 並びは名前の順で、集合の順によらない
        self.assertLess(document.index("[alpha]"), document.index("[beta]"))

    def test_crate_without_a_license_file_gets_the_standard_text(self):
        crates = {self.crate("bare", "0.1.0", "MIT/Apache-2.0", authors=["Example Ltd. <dev@example.org>"])}
        document = rust.render(crates, self.packages)
        self.assertIn("| MIT OR Apache-2.0 | T1 |", document)
        self.assertIn("Copyright (c) Example Ltd. <dev@example.org>", document)
        self.assertIn("Used by: bare 0.1.0 (MIT (standard text))", document)

    def test_standard_apache_text_is_available(self):
        crates = {self.crate("apache-only", "0.1.0", "Apache-2.0")}
        document = rust.render(crates, self.packages)
        self.assertIn("apache-only 0.1.0 (Apache-2.0 (standard text))", document)
        self.assertIn("TERMS AND CONDITIONS FOR USE, REPRODUCTION, AND DISTRIBUTION", document)

    def test_unreviewed_license_stops(self):
        crates = {self.crate("copyleft", "1.0.0", "GPL-2.0-only", {"LICENSE": "GPL\n"})}
        with self.assertRaisesRegex(rust.NoticeError, "GPL-2.0-only is not in ALLOWED_LICENSES"):
            rust.render(crates, self.packages)

    def test_missing_license_and_missing_text_stop(self):
        with self.assertRaisesRegex(rust.NoticeError, "declares no license"):
            rust.render({self.crate("unlicensed", "1.0.0", None)}, self.packages)
        with self.assertRaisesRegex(rust.NoticeError, "no standard text is available for ISC"):
            rust.render({self.crate("isc-bare", "1.0.0", "ISC")}, self.packages)

    def test_missing_source_stops(self):
        key = self.crate("gone", "1.0.0", "MIT", {"LICENSE": "MIT\n"})
        shutil.rmtree(self.root / "gone-1.0.0")
        with self.assertRaisesRegex(rust.NoticeError, "cargo fetch --locked"):
            rust.render({key}, self.packages)

    def test_workspace_crates_are_not_listed(self):
        crates = {("sbq-protocol", "0.1.0"), self.crate("dep", "1.0.0", "MIT", {"LICENSE": "MIT\n"})}
        document = rust.render(crates, self.packages)
        self.assertIn("## Crates (1)", document)
        self.assertNotIn("sbq-protocol", document)

    def test_text_with_code_fences_stays_verbatim(self):
        crates = {self.crate("fenced", "1.0.0", "MIT", {"LICENSE": "Example:\n```\ncode\n```\n"})}
        document = rust.render(crates, self.packages)
        self.assertIn("````text\nExample:\n```\ncode\n```\n````", document)


class CommittedDocumentTest(unittest.TestCase):
    def test_document_matches_cargo_lock(self):
        """resources/licenses/RUST-NOTICES.md is what Cargo.lock gives (rerun the script after changing dependencies)."""
        if shutil.which("cargo") is None:
            self.skipTest("cargo is not installed")
        try:
            expected = rust.generate(offline=True)
        except rust.NoticeError as error:
            if "cargo fetch" in str(error) or "--offline" in str(error) or "offline" in str(error):
                self.skipTest(f"crate sources are not in the local cargo registry: {error}")
            raise
        self.assertEqual(rust.DEFAULT_OUTPUT.read_text(encoding="utf-8"), expected,
                         "Run `python3 scripts/rust_licenses.py` and commit resources/licenses/RUST-NOTICES.md")


if __name__ == "__main__":
    unittest.main()
