"""並列監査の集約・再開・異常終了を模擬検証器で確認する。"""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class AuditCollectionsTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.auditor = self.root / "auditor"
        self.auditor.write_text("""#!/usr/bin/env python3
import json, sys
for line in sys.stdin:
    request = json.loads(line)
    if request['sfen'] == 'crash':
        sys.exit(2)
    status = 'unknown' if request['sfen'] == 'unknown' else 'minimal'
    print(json.dumps({'original':request['sfen'], 'target':request['target'], 'status':status,
                      'sfen':'canonical b - 1', 'pv':['a','b','c'], 'removed':1}), flush=True)
""")
        self.auditor.chmod(0o755)
        self.inputs = self.root / "input.txt"
        self.inputs.write_text("# test\na moves a b c\nb moves a b c\nunknown moves a b c\n")
        self.output = self.root / "output"

    def run_audit(self):
        script = Path(__file__).resolve().parents[1] / "scripts/audit_tsume_collections.py"
        return subprocess.run([sys.executable, str(script), str(self.inputs),
                               "--auditor", str(self.auditor), "--engine", str(self.auditor),
                               "--output-dir", str(self.output), "--workers", "2"],
                              capture_output=True, text=True, timeout=20)

    def test_deduplicate_minimal_results_and_resume(self):
        first = self.run_audit()
        self.assertEqual(first.returncode, 0, first.stderr)
        summary = json.loads((self.output / "summary.json").read_text())["3"]
        self.assertEqual(summary["statuses"], {"minimal": 2, "unknown": 1})
        self.assertEqual(summary["unique_minimal"], 1)
        self.assertEqual((self.output / "tsume_3ply_minimal.txt").read_text(),
                         "canonical b - 1 moves a b c\n")
        before = (self.output / "audit.jsonl").read_text()
        second = self.run_audit()
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual((self.output / "audit.jsonl").read_text(), before)

    def test_changed_auditor_does_not_reuse_old_certificates(self):
        self.assertEqual(self.run_audit().returncode, 0)
        with self.auditor.open("a") as stream:
            stream.write("\n# Changed verifier\n")
        result = self.run_audit()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("検証器が変わっています", result.stderr)

    def test_child_failure_is_not_counted_as_success(self):
        self.inputs.write_text("crash moves a b c\n")
        result = self.run_audit()
        self.assertNotEqual(result.returncode, 0)
        summary = json.loads((self.output / "summary.json").read_text())["3"]
        self.assertEqual(summary["unique_minimal"], 0)
        self.assertEqual(summary["statuses"], {})


if __name__ == "__main__":
    unittest.main()
