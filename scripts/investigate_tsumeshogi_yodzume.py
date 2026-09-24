#!/usr/bin/env python3
"""余詰調査用。製品コードは変更せず、Hayanagi と python-shogi で照合する。

python-shogi==1.1.1 が必要。実行例は調査報告を参照。
Hayanagi の全初手探索と証明木を JSON に保存し、証明木の全玉方応手、
各着手の合法性、終端の詰みを独立した python-shogi で確認する。
独立探索では 1/3 手以内の詰みの有無のみ調べる（5/7 手の不存在は証明しない）。
"""

import argparse
import gzip
import hashlib
import importlib.metadata
import importlib.util
import json
from pathlib import Path
import platform
import re
import subprocess
import sys
import time

import shogi


ROOT = Path(__file__).resolve().parents[1]
# 読取専用で使うサブモジュールに __pycache__ を作らない。
sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location("hayanagi_test_engine", ROOT / "Hayanagi/tests/test_engine.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def git_head(path):
    return subprocess.check_output(["git", "-C", str(path), "rev-parse", "HEAD"], text=True).strip()


def checking_moves(board):
    result = []
    for move in list(board.legal_moves):
        board.push(move)
        check = board.is_check()
        board.pop()
        if check:
            result.append(move)
    return sorted(result, key=lambda move: move.usi())


def bounded_mate(board, remaining, attacker, deadline):
    """独立した有限 AND/OR 探索。False は指定深さ内の不存在のみを表す。"""
    if time.monotonic() >= deadline:
        raise TimeoutError("independent search timed out")
    attack = board.turn == attacker
    if not attack and not board.is_check():
        return False
    if not attack:
        moves = list(board.legal_moves)
        if not moves:
            return True
    if remaining == 0:
        return False
    if attack:
        moves = checking_moves(board)
    for move in moves:
        board.push(move)
        try:
            result = bounded_mate(board, remaining - 1, attacker, deadline)
        finally:
            board.pop()
        if attack and result:
            return True
        if not attack and not result:
            return False
    return not attack


class Audit:
    def __init__(self, engine, millis):
        self.engine = engine
        self.millis = millis
        self.queries = {}
        self.proof = {}
        self.legal_checks = 0

    def solve(self, board, remaining):
        key = board.sfen() + " | " + str(remaining)
        if key not in self.queries:
            side = "attack" if board.turn == shogi.BLACK else "defense"
            self.engine.send("position sfen " + board.sfen())
            self.engine.send(f"go tsume {side} depth {remaining} movetime {self.millis}")
            line = self.engine.until("tsume ", timeout=self.millis / 1000 + 5)[-1]
            fields = line.split()
            data = dict(zip(fields[2::2], fields[3::2]))
            self.queries[key] = {"status": fields[1], **data, "raw": line}
        return self.queries[key]

    def compare_legal(self, board):
        self.engine.send("position sfen " + board.sfen())
        lines, _ = self.engine.perft(1, divide=True)
        actual = sorted(line.split(":")[0] for line in lines if re.match(
            r"^(?:[1-9][a-i][1-9][a-i]\+?|[PLNSGBR]\*[1-9][a-i]):", line))
        expected = sorted(move.usi() for move in board.legal_moves)
        if actual != expected:
            raise AssertionError({"sfen": board.sfen(), "hayanagi": actual, "python_shogi": expected})
        self.legal_checks += 1
        return expected

    def attack_options(self, board, remaining):
        assert board.turn == shogi.BLACK and remaining > 0
        options = {}
        for move in checking_moves(board):
            board.push(move)
            try:
                options[move.usi()] = self.solve(board, remaining - 1)
            finally:
                board.pop()
        return options

    def prove(self, board, remaining):
        """攻方の全詰手と玉方の全合法手を保存。玉方分岐の欠落は独立に検出。"""
        key = board.sfen() + " | " + str(remaining)
        if key in self.proof:
            return key
        legal = self.compare_legal(board)
        attack = board.turn == shogi.BLACK
        node = {"sfen": board.sfen(), "remaining": remaining,
                "side": "attack" if attack else "defense", "legal_moves": legal, "children": {}}
        self.proof[key] = node
        if not attack:
            assert board.is_check(), key
            if not legal:
                node["terminal_mate"] = True
                return key
            choices = legal
        else:
            node["options"] = self.attack_options(board, remaining)
            choices = [move for move, result in node["options"].items() if result["status"] == "mate"]
            assert choices, key
        assert remaining > 0, key
        for text in choices:
            move = shogi.Move.from_usi(text)
            assert move in board.legal_moves, (key, text)
            board.push(move)
            try:
                if attack:
                    assert board.is_check(), (key, text)
                result = self.solve(board, remaining - 1)
                assert result["status"] == "mate", (key, text, result)
                node["children"][text] = self.prove(board, remaining - 1)
            finally:
                board.pop()
        return key


def verify_proof(proof, roots):
    """保存された証明木だけを python-shogi で再検証（Hayanagi 呼出しなし）。"""
    verified = set()

    def visit(key):
        if key in verified:
            return
        node = proof[key]
        board = shogi.Board(node["sfen"])
        legal = {move.usi() for move in board.legal_moves}
        assert legal == set(node["legal_moves"]), key
        attack = board.turn == shogi.BLACK
        children = node["children"]
        if attack:
            assert children and set(children) <= legal, key
        else:
            assert board.is_check() and set(children) == legal, key
        if node["remaining"] == 0:
            assert not attack and not legal, key
        for text, child_key in children.items():
            board.push_usi(text)
            assert not attack or board.is_check(), (key, text)
            child = proof[child_key]
            assert child["sfen"] == board.sfen(), key
            assert child["remaining"] == node["remaining"] - 1, key
            visit(child_key)
            board.pop()
        verified.add(key)

    for key in roots:
        visit(key)
    return len(verified)


def verify_sequence(sfen, sequence):
    board = shogi.Board(sfen)
    for text in sequence.split():
        move = shogi.Move.from_usi(text)
        assert move in board.legal_moves, (sfen, sequence, text)
        attack = board.turn == shogi.BLACK
        board.push(move)
        assert not attack or board.is_check(), (sequence, text)
    assert board.turn == shogi.WHITE and board.is_checkmate(), sequence
    return {"moves": sequence, "legal_continuous_checks_and_terminal_mate": True}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, default=ROOT / "build/Hayanagi/hayanagi")
    parser.add_argument("--output", type=Path)
    parser.add_argument("--verify-proof", type=Path, help="保存済み JSON/JSON.gz の証明木だけを独立再検証")
    parser.add_argument("--millis", type=int, default=10000)
    args = parser.parse_args()
    if args.verify_proof:
        opener = gzip.open if args.verify_proof.suffix == ".gz" else open
        with opener(args.verify_proof, "rt", encoding="utf-8") as stream:
            data = json.load(stream)
        roots = [problem["proof_root"] for problem in data["problems"]]
        roots += [problem["longer_S4b"]["proof_root"] for problem in data["problems"] if "longer_S4b" in problem]
        print("verified proof nodes:", verify_proof(data["proof"], roots))
        return
    if args.output is None:
        parser.error("--output または --verify-proof を指定してください")
    engine_path = args.engine.resolve()
    sfens = (ROOT / "tests/fixtures/tsume_positions.sfen").read_text().splitlines()
    references = (ROOT / "tests/fixtures/tsume_positions_with_moves.sfen").read_text().splitlines()
    assert [line.split(" moves ")[0] for line in references] == sfens
    output = {"metadata": {
        "shogiboardq_commit": git_head(ROOT), "hayanagi_commit": git_head(ROOT / "Hayanagi"),
        "engine": str(engine_path), "engine_sha256": sha256(engine_path),
        "python": platform.python_version(), "python_shogi": importlib.metadata.version("python-shogi"),
        "python_shogi_source_sha256": sha256(Path(shogi.__file__)),
        "millis_per_query": args.millis,
        "independent_scope": "all positive proof trees + no mate within 1/3 plies; not exhaustive negative depth 5/7",
    }, "fixtures": {}, "problems": []}
    for desktop_name, fixture_name in [("5手詰.txt", "tsume_positions.sfen"),
                                       ("5手詰2.txt", "tsume_positions_with_moves.sfen")]:
        fixture = ROOT / "tests/fixtures" / fixture_name
        desktop = Path.home() / "Desktop" / desktop_name
        output["fixtures"][fixture_name] = {"sha256": sha256(fixture),
            "desktop_exists": desktop.exists(),
            "desktop_bytes_equal": desktop.read_bytes() == fixture.read_bytes() if desktop.exists() else None}

    engine = MODULE.Engine(engine_path)
    try:
        engine.send("usi")
        output["metadata"]["usi_handshake"] = engine.until("usiok")
        options = ["setoption name TsumeMode value true", "setoption name USI_OwnBook value false",
                   "setoption name GenerateAllLegalMoves value true"]
        output["metadata"]["explicit_options"] = options
        for option in options:
            engine.send(option)
        engine.ready()
        audit = Audit(engine, args.millis)
        roots = []
        for index, (sfen, reference) in enumerate(zip(sfens, references), 1):
            start = time.monotonic()
            board = shogi.Board(sfen)
            audit.compare_legal(board)
            depths = {str(depth): audit.solve(board, depth) for depth in (1, 3, 5)}
            independent = {}
            for depth in (1, 3):
                try:
                    independent[str(depth)] = bounded_mate(board, depth, shogi.BLACK, time.monotonic() + 60)
                except TimeoutError:
                    independent[str(depth)] = "timeout"
            root = audit.prove(board, 5)
            roots.append(root)
            sequences = [reference.split(" moves ")[1]]
            if index == 2:
                sequences += ["R*9g 8g8f 9g9f 8f8e G*8f", "R*8h 8g7f G*6f 7f7g 6i7h"]
            if index == 4:
                sequences += ["R*4b 4a5a 4b6b+ 5a4a 2d3b+"]
            problem = {"number": index, "sfen": sfen, "depth_results": depths,
                       "independent_bounded_results": independent, "proof_root": root,
                       "root_options": audit.proof[root]["options"],
                       "sequences": [verify_sequence(sfen, sequence) for sequence in sequences]}
            if index == 4:
                board.push_usi("S*4b")
                problem["longer_S4b"] = {str(depth): audit.solve(board, depth) for depth in (4, 6)}
                problem["longer_S4b"]["proof_root"] = audit.prove(board, 6)
                roots.append(problem["longer_S4b"]["proof_root"])
                board.pop()
            problem["seconds"] = round(time.monotonic() - start, 3)
            output["problems"].append(problem)
            winners = [move for move, result in problem["root_options"].items() if result["status"] == "mate"]
            print(f"problem {index}: winners={winners}, independent={independent}, seconds={problem['seconds']}", flush=True)
        output["proof"] = audit.proof
        output["queries"] = audit.queries
        output["legal_move_sets_compared"] = audit.legal_checks
        output["independently_verified_proof_nodes"] = verify_proof(audit.proof, roots)
        # 現行 Hayanagi の標準 go mate 応答を、独自 go tsume と区別して記録する。
        engine.send("position sfen " + sfens[1])
        engine.send("go mate 1000")
        output["hayanagi_go_mate_probe"] = engine.until("bestmove ")
    finally:
        engine.close()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    payload = (json.dumps(output, ensure_ascii=False, indent=2) + "\n").encode("utf-8")
    args.output.write_bytes(gzip.compress(payload, mtime=0) if args.output.suffix == ".gz" else payload)
    print(f"saved {args.output}; verified {output['independently_verified_proof_nodes']} proof nodes", flush=True)


if __name__ == "__main__":
    main()
