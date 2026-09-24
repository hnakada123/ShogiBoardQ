#!/usr/bin/env python3
"""指定された KomoringHeights で go mate 応答を再現し、全通信を保存する。"""

import argparse
import json
from pathlib import Path
import sys
import time

import shogi

sys.dont_write_bytecode = True
from investigate_tsumeshogi_yodzume import MODULE, ROOT, sha256


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--hash-mb", type=int, default=4096)
    parser.add_argument("--threads", type=int, default=4)
    parser.add_argument("--millis", type=int, default=5000)
    args = parser.parse_args()
    sfens = (ROOT / "tests/fixtures/tsume_positions.sfen").read_text().splitlines()
    cases = [(i, "") for i in range(1, 6)]
    cases += [(2, move) for move in ("G*9g", "R*9g", "R*8h")]
    cases += [(4, move) for move in ("S*3b", "R*4b", "S*4b")]
    cases += [(4, "S*3b 4a4b " + move) for move in ("6d5c", "R*4a", "R*4c")]
    output = {"engine": str(args.engine.resolve()), "sha256": sha256(args.engine),
              "options": {"Threads": args.threads, "USI_Hash": args.hash_mb,
                          "MultiPV": 1, "GenerateAllLegalMoves": "true", "NodesLimit": 0,
                          "RootIsAndNodeIfChecked": "true", "PostSearchLevel": "MinLength",
                          "PvInterval": 1000, "ScoreCalculation": "Ponanza", "WriteDebugLog": ""},
              "millis": args.millis, "cases": []}
    engine = MODULE.Engine(args.engine.resolve())
    try:
        engine.send("usi")
        output["handshake"] = engine.until("usiok")
        for name, value in output["options"].items():
            engine.send(f"setoption name {name} value {value}")
        output["ready"] = engine.ready()
        for number, moves in cases:
            # 各問を新規対局として扱い、探索済み置換表の影響を避ける。
            engine.send("usinewgame")
            engine.ready()
            position = "position sfen " + sfens[number - 1] + (" moves " + moves if moves else "")
            engine.send(position)
            start = time.monotonic()
            engine.send(f"go mate {args.millis}")
            lines = engine.until("checkmate ", timeout=args.millis / 1000 + 10)
            result = {"problem": number, "moves": moves, "position_command": position,
                      "response": lines[-1], "lines": lines, "seconds": round(time.monotonic() - start, 3)}
            pv = lines[-1].split()[1:]
            if pv and pv[0] not in ("nomate", "timeout", "notimplemented"):
                board = shogi.Board(sfens[number - 1])
                for move in moves.split() + pv:
                    legal = shogi.Move.from_usi(move)
                    assert legal in board.legal_moves, (number, moves, lines[-1], move)
                    attack = board.turn == shogi.BLACK
                    board.push(legal)
                    assert not attack or board.is_check(), (number, moves, move)
                assert board.turn == shogi.WHITE and board.is_checkmate(), result
                result["pv_plies"] = len(pv)
                result["independent_pv_legality_and_terminal_mate"] = True
            output["cases"].append(result)
            print(f"problem {number} {moves or '(root)'}: {lines[-1]}", flush=True)
    finally:
        engine.close()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n")


if __name__ == "__main__":
    main()
