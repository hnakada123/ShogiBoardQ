#!/usr/bin/env python3
"""独立した最終監査を通った問題集を、検査記録・類似判定付きで出力する。"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path

from refill_diverse_tsume import atomic
from tsume_diversity import Problem, metrics, select, spread


def publish(args):
    binary_hash = hashlib.sha256(args.auditor.read_bytes()).hexdigest()
    latest = {}
    for path in args.audit_logs:
        for line in path.read_text().splitlines():
            record = json.loads(line)
            if record["auditor_sha256"] != binary_hash:
                raise ValueError(f"検査器のハッシュが異なります: {path}")
            latest[(record["original"], record["target"])] = record
    # 後から判定不能になった局面に、以前の合格証明を適用しない。
    # 最終監査で変更が発生した問題も、改めて監査を完了するまで公開しない。
    records = {key: record for key, record in latest.items()
               if record["status"] == "minimal" and record["removed"] == 0
               and record["original"] == record["sfen"] and record["legal_pv"]
               and record["shortest_plies"] == record["target"]
               and len(record["pv"]) == record["target"]}
    args.output_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.now(timezone.utc).isoformat(timespec="seconds")
    report = {"validated_at": stamp, "method": "all_single_piece_removals_to_defender_hand",
              "allow_final_move_alternatives": True, "auditor_sha256": binary_hash,
              "engine": "KomoringHeights 1.1.0 64ZEN2 (MinLength)",
              "independent_shortest_search": "Hayanagi TsumeSearch with a fresh table per SFEN",
              "diversity_filter_sha256": hashlib.sha256(Path(__file__).with_name("tsume_diversity.py").read_bytes()).hexdigest(),
              "diversity_policy": "mirrored_relative_prefix_and_long_core_one_move_difference_v1",
              "files": []}
    outputs = []
    for path in args.originals:
        original = [Problem.parse(l) for l in path.read_text().splitlines() if l and not l.startswith("#")]
        n = len(original[0].pv)
        candidates = [Problem(sfen, tuple(record["pv"])) for (sfen, target), record in records.items() if target == n]
        index = select(candidates)
        if len(index.problems) != args.count or len(candidates) != args.count:
            raise ValueError(f"{n}手詰は検証済み{len(candidates)}題、類似除外後{len(index.problems)}題です（必要{args.count}題）")
        ordered = spread(index.problems)
        check_count = sum(len(records[(p.sfen, n)]["checks"]) for p in ordered)
        header = [f"# ShogiBoardQ — {n}手詰 {args.count}題", "# Format: SFEN moves USI...",
                  f"# Diversity revision and independent validation: {stamp}",
                  "# Candidates: random placement, seed mutation, and reverse construction of preceding checks.",
                  "# Shared diversity filter across all workers; compared before and after trimming.",
                  "# Reflection/translation, gold-equivalent substitutions and final-move variants excluded.",
                  "# For >=9 plies, also exclude matching middle lines with at most one different move.",
                  "# Every retained position: exact shortest mate, legal PV, main-line uniqueness,",
                  "# and conclusive verification of all single-piece removals to defender hand.",
                  "# Final-move alternatives and shorter side-line alternatives are allowed.",
                  f"# Single-piece removals checked: {check_count}",
                  "# Order chosen to separate similar move sequences in the preceding 10 problems.",
                  "# Problem numbers changed in this revision. See README.md and validation_20260926.json.",
                  "#"]
        text = "\n".join(header + [p.line for p in ordered]) + "\n"
        original_sfens = {p.sfen for p in original}
        retained = sum(p.sfen in original_sfens for p in ordered)
        report["files"].append({"file": path.name, "sha256": hashlib.sha256(text.encode()).hexdigest(),
            "previous_sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "target_plies": n,
            "positions": len(ordered), "unique_positions": len({p.sfen for p in ordered}),
            "legal_mate_lines": len(ordered), "shortest_mate_exact_target": len(ordered),
            "positions_with_removable_piece": 0, "inconclusive_positions": 0,
            "single_piece_removals_checked": check_count, "retained_previous_positions": retained,
            "replacement_positions": len(ordered) - retained,
            "before_diversity": metrics(original), "after_diversity": metrics(ordered)})
        outputs.append((args.output_dir / path.name, text))
    report["total_positions"] = sum(f["positions"] for f in report["files"])
    report["total_single_piece_removals_checked"] = sum(f["single_piece_removals_checked"] for f in report["files"])
    # 全問題集の検査が揃ってから書き出す。不足時に一部だけ公開しない。
    for path, text in outputs:
        atomic(path, text)
    atomic(args.output_dir / "validation_20260926.json", json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("originals", nargs="+", type=Path)
    parser.add_argument("--audit-logs", nargs="+", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    parser.add_argument("--auditor", type=Path, default=Path("build/tests/tsumeshogi_collection_auditor"))
    parser.add_argument("--count", type=int, default=1000)
    publish(parser.parse_args())
