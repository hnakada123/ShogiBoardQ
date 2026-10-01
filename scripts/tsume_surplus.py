"""保存手順の駒取り・駒打ちを再生して、攻方の詰め上がりの持駒を数える。

王手・合法性・詰みは collection_auditor で別途検証する。
"""
from collections import Counter
import re

from tsume_diversity import board_from_sfen, square


def remaining_hand(problem):
    board = board_from_sfen(problem.sfen)
    hands = Counter()
    for count, piece in re.findall(r"(\d*)([PLNSGBRplnsgbr])", problem.sfen.split()[2]):
        hands[piece] += int(count or 1)
    for ply, move in enumerate(problem.pv):
        black = ply % 2 == 0
        dest = square(move[2:4])
        if move[1] == "*":
            piece = move[0] if black else move[0].lower()
            if dest in board or hands[piece] <= 0:
                raise ValueError("駒打ちの再生に失敗しました")
            hands[piece] -= 1
        else:
            piece = board.pop(square(move[:2]))
            if piece[-1].isupper() != black:
                raise ValueError("手番と移動する駒が一致しません")
            if dest in board:
                captured = board[dest][-1]
                if captured.isupper() == black or captured.upper() == "K":
                    raise ValueError("駒取りの再生に失敗しました")
                hands[captured.upper() if black else captured.lower()] += 1
            if move.endswith("+"):
                piece = "+" + piece
        board[dest] = piece
    return {piece: count for piece, count in sorted(hands.items()) if piece.isupper() and count}


def inventory(inputs, output_dir):
    """元ファイルを保持し、再生結果・駒余りのない種・除外SFENを作業先に保存する。"""
    import json
    from tsume_diversity import Problem

    output_dir.mkdir(parents=True, exist_ok=True)
    report, excluded = [], []
    for path in inputs:
        problems = [Problem.parse(line) for line in path.read_text(encoding="utf-8").splitlines()
                    if line.strip() and not line.startswith("#")]
        targets = {len(p.pv) for p in problems}
        if len(targets) != 1:
            raise ValueError(f"{path}: 手数が混在または問題が空です")
        target = targets.pop()
        clean, surplus = [], []
        for number, problem in enumerate(problems, 1):
            hand = remaining_hand(problem)
            if hand:
                surplus.append(dict(number=number, sfen=problem.sfen, pv=problem.pv, remaining=hand))
                excluded.append(problem.sfen)
            else:
                clean.append(problem.line)
        (output_dir / f"tsume_{target}ply_clean.txt").write_text("\n".join(clean) + "\n", encoding="utf-8")
        report.append(dict(file=path.name, target=target, count=len(problems), clean=len(clean),
                           surplus=len(surplus), problems=surplus))
    (output_dir / "original-surplus.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    (output_dir / "excluded-originals.txt").write_text("\n".join(excluded) + "\n", encoding="utf-8")
    print(json.dumps([{k: v for k, v in r.items() if k != "problems"} for r in report], ensure_ascii=False, indent=2))


if __name__ == "__main__":
    import argparse
    from pathlib import Path

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="+", type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()
    inventory(args.inputs, args.output_dir)
