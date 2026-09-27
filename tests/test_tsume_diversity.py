"""類似作の回帰テストと、並列補充の再検査・再開・異常終了の確認。"""
import json
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from tsume_diversity import Problem, board_from_sfen, select, spread
from publish_diverse_tsume import publish
from refill_diverse_tsume import seed_sample

LINE_722 = "1+N+P6/1Pp6/1kL6/9/2S6/9/9/9/9 b G2r2b3g3s3n3l15p 1 moves G*8d 8c9b 8a9a 9b9a 8b8a+ 9a9b 8a8b 9b8b 7c7b+ 8b9a 7a8a 9a9b 8a8b"
LINE_723 = "1G+P6/1Pp6/1kP6/9/1S7/9/9/9/9 b G2r2b2g3s4n4l14p 1 moves G*8d 8c9b 8a9a 9b9a 8b8a+ 9a9b 8a8b 9b8b 7c7b+ 8b9a 7a8a 9a9b 8a8b"
SHORT = [
    "9/8k/7P1/9/7R1/9/9/9/9 b r2b4g4s4n4l17p 1 moves 2c2b+ 1b1c 2e2c+",
    "9/9/9/9/9/9/1+P7/k8/2+N6 b 2r2b4g4s3n4l17p 1 moves 8g8h 9h9i 8h8i",
    "1k7/2p1+N4/P8/L8/9/9/9/9/9 b B2rb4g4s3n3l16p 1 moves 9c9b+ 8a7a B*8b",
]


def transform(problem, file_map):
    board = {(file_map(x), y): p for (x, y), p in board_from_sfen(problem.sfen).items()}
    rows = []
    for y in range(9):
        row, blanks = "", 0
        for x in range(9, 0, -1):
            if (x, y) not in board:
                blanks += 1
                continue
            row += (str(blanks) if blanks else "") + board[(x, y)]
            blanks = 0
        rows.append(row + (str(blanks) if blanks else ""))
    def move_map(move):
        chars = list(move)
        for i in ([2] if move[1] == "*" else [0, 2]):
            chars[i] = str(file_map(int(chars[i])))
        return "".join(chars)
    return Problem("/".join(rows) + " " + " ".join(problem.sfen.split()[1:]),
                   tuple(move_map(m) for m in problem.pv))


class DiversityTest(unittest.TestCase):
    def test_reported_pair_and_gold_equivalent_substitutions(self):
        self.assertEqual(len(select([Problem.parse(LINE_722), Problem.parse(LINE_723)]).problems), 1)

    def test_reflection_and_translation(self):
        p = Problem.parse(LINE_722)
        self.assertEqual(len(select([p, transform(p, lambda x: 10 - x),
                                    transform(p, lambda x: x - 4)]).problems), 1)

    def test_king_start_and_final_alternatives(self):
        p = Problem.parse(LINE_722)
        different = Problem.parse(LINE_723.replace("1kP6", "k1P6").replace("8c9b", "9c9b"))
        final = Problem(p.sfen, p.pv[:-1] + ("7b8b",))
        self.assertEqual(len(select([p, different, final]).problems), 1)

    def test_distinct_motifs_are_kept(self):
        problems = [Problem.parse(line) for line in SHORT]
        self.assertEqual(select(problems).problems, problems)

    def test_order_preserves_all_problems_and_is_reproducible(self):
        problems = [Problem.parse(line) for line in SHORT]
        self.assertCountEqual(spread(problems), problems)
        self.assertEqual(spread(problems), spread(list(reversed(problems))))

    def test_bad_input_is_not_silently_accepted(self):
        for line in ("9/9 b - 1 moves 7g7f", "9/9/9/9/9/9/9/9/9 b - 1 moves bad"):
            with self.assertRaises(ValueError):
                Problem.parse(line)

    def test_seed_selection_includes_new_families_and_rotates_history(self):
        group = list(range(1000))
        first, second = seed_sample(group, 200, 0), seed_sample(group, 200, 137)
        self.assertEqual(len(set(first)), 200)
        self.assertTrue(set(range(900, 1000)).issubset(first))
        self.assertNotEqual(set(first), set(second))


class RefillTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.input = self.root / "input.txt"
        self.input.write_text(SHORT[0] + "\n")
        self.sampler = self.root / "sampler"
        candidates = [Problem.parse(line) for line in SHORT[1:]]
        events = [{"sfen": p.sfen, "pv": p.pv, "target": 3, "source": "mock"} for p in candidates]
        self.sampler.write_text("#!/usr/bin/env python3\nimport json\nfor event in " + repr(events) + ":\n print(json.dumps(event),flush=True)\n")
        self.sampler.chmod(0o755)
        self.auditor = self.root / "auditor"
        clean = candidates[0]
        self.auditor.write_text("#!/usr/bin/env python3\nimport json,sys\nfor line in sys.stdin:\n r=json.loads(line)\n print(json.dumps(dict(original=r['sfen'],target=r['target'],status='minimal',sfen="
                                + repr(clean.sfen) + ",pv=" + repr(clean.pv) + ")),flush=True)\n")
        self.auditor.chmod(0o755)

    def run_refill(self, *extra):
        return subprocess.run([sys.executable, str(ROOT / "scripts/refill_diverse_tsume.py"), str(self.input),
                               "--sampler", str(self.sampler), "--auditor", str(self.auditor),
                               "--engine", str(self.auditor), "--output-dir", str(self.root / "output"),
                               "--workers", "2", "--count", "3", "--seconds", "1", *extra],
                              capture_output=True, text=True, timeout=20)

    def test_concurrent_results_rechecked_after_trimming_and_resume(self):
        first = self.run_refill()
        self.assertEqual(first.returncode, 0, first.stderr)
        output = self.root / "output"
        result = json.loads((output / "progress.json").read_text())["files"]["3"]
        self.assertEqual(result["current"], 2)
        self.assertEqual(result["missing"], 1)
        before = (output / "audit.jsonl").read_text()
        second = self.run_refill()
        self.assertEqual(second.returncode, 0, second.stderr)
        self.assertEqual(before, (output / "audit.jsonl").read_text())

    def test_dead_auditor_is_failure(self):
        self.auditor.write_text("#!/usr/bin/env python3\nimport sys\nsys.exit(2)\n")
        result = self.run_refill()
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(json.loads((self.root / "output/progress.json").read_text())["files"]["3"]["current"], 1)

    def test_final_audit_exclusion_survives_resume(self):
        self.assertEqual(self.run_refill().returncode, 0)
        excluded = self.root / "excluded.txt"
        excluded.write_text(Problem.parse(SHORT[1]).sfen + "\n")
        result = self.run_refill("--exclude-sfens", str(excluded))
        self.assertEqual(result.returncode, 0, result.stderr)
        output = self.root / "output/tsume_3ply_working.txt"
        self.assertEqual(output.read_text().splitlines(), [SHORT[0]])
        self.assertEqual(self.run_refill().returncode, 0)
        self.assertEqual(output.read_text().splitlines(), [SHORT[0]])

    def test_same_sfen_is_not_counted_twice_when_returned_pv_changes(self):
        original, changed = (Problem.parse(line) for line in SHORT[:2])
        self.auditor.write_text("#!/usr/bin/env python3\nimport json,sys\nfor line in sys.stdin:\n"
            " r=json.loads(line)\n print(json.dumps(dict(original=r['sfen'],target=3,status='minimal',sfen="
            + repr(original.sfen) + ",pv=" + repr(changed.pv) + ")),flush=True)\n")
        result = self.run_refill()
        self.assertEqual(result.returncode, 0, result.stderr)
        saved = (self.root / "output/tsume_3ply_working.txt").read_text().splitlines()
        self.assertEqual(saved, [SHORT[0]])

    def test_dead_sampler_is_failure(self):
        self.sampler.write_text("#!/usr/bin/env python3\nimport sys\nsys.exit(2)\n")
        result = self.run_refill()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("候補生成器", result.stderr)

    def test_multiple_candidate_is_only_accepted_after_repair_audit(self):
        bad, repaired = (Problem.parse(line) for line in SHORT[1:])
        self.sampler.write_text("#!/usr/bin/env python3\nimport json,sys\n"
            "if sys.argv[1:] == ['--removals']:\n"
            " for line in sys.stdin: print(json.dumps(" + repr([repaired.sfen]) + "),flush=True)\n"
            "else:\n print(json.dumps(" + repr(dict(sfen=bad.sfen, pv=bad.pv, target=3, source="mock")) + "),flush=True)\n")
        self.auditor.write_text("#!/usr/bin/env python3\nimport json,sys\nfor line in sys.stdin:\n"
            " r=json.loads(line)\n status='minimal' if r['sfen']==" + repr(repaired.sfen) + " else 'multiple'\n"
            " print(json.dumps(dict(original=r['sfen'],target=r['target'],status=status,sfen="
            + repr(repaired.sfen) + ",pv=" + repr(repaired.pv) + ")),flush=True)\n")
        result = self.run_refill()
        self.assertEqual(result.returncode, 0, result.stderr)
        saved = (self.root / "output/tsume_3ply_working.txt").read_text().splitlines()
        self.assertEqual(saved, [SHORT[0], repaired.line])
        records = [json.loads(line) for line in (self.root / "output/audit.jsonl").read_text().splitlines()]
        self.assertEqual([r["status"] for r in records], ["multiple", "minimal"])
        self.assertEqual(records[1]["source"], "repair:mock")

    def test_publish_requires_latest_conclusive_audit(self):
        problem = Problem.parse(SHORT[0])
        log = self.root / "audit.jsonl"
        common = dict(original=problem.sfen, target=3,
                      auditor_sha256=hashlib.sha256(self.auditor.read_bytes()).hexdigest())
        passed = dict(common, status="minimal", removed=0, sfen=problem.sfen,
                      legal_pv=True, shortest_plies=3, pv=problem.pv, checks=[])
        log.write_text(json.dumps(passed) + "\n" + json.dumps(dict(common, status="unknown")) + "\n")
        output = self.root / "publish"
        args = SimpleNamespace(auditor=self.auditor, audit_logs=[log], originals=[self.input],
                               output_dir=output, count=1)
        with self.assertRaises(ValueError):
            publish(args)
        self.assertFalse((output / "input.txt").exists())


if __name__ == "__main__":
    unittest.main()
