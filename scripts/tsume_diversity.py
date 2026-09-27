"""問題集用の保守的な類似作フィルター。詰みの正当性は別途実エンジンで検証する。"""
from dataclasses import dataclass
from collections import Counter
import re

MOVE = re.compile(r"(?:[PLNSGBR]\*[1-9][a-i]|[1-9][a-i][1-9][a-i]\+?)\Z")


def board_from_sfen(sfen):
    fields = sfen.split()
    if len(fields) != 4 or fields[1] != "b":
        raise ValueError("先手番のSFENが必要です")
    board = {}
    ranks = fields[0].split("/")
    if len(ranks) != 9:
        raise ValueError("盤面の段数が不正です")
    for y, row in enumerate(ranks):
        x, promoted = 9, False
        for ch in row:
            if ch == "+":
                if promoted:
                    raise ValueError("成駒表記が不正です")
                promoted = True
            elif ch in "123456789":
                if promoted:
                    raise ValueError("成駒表記が不正です")
                x -= int(ch)
            elif ch in "PLNSGBRKplnsgbrk":
                if x < 1 or (promoted and ch.upper() in "GK"):
                    raise ValueError("駒配置が不正です")
                board[(x, y)] = ("+" if promoted else "") + ch
                x -= 1
                promoted = False
            else:
                raise ValueError("SFENに不正な文字があります")
        if x != 0 or promoted:
            raise ValueError("盤面の筋数が不正です")
    if list(board.values()).count("k") != 1:
        raise ValueError("後手玉が1枚必要です")
    return board


def square(text):
    return int(text[0]), ord(text[1]) - ord("a")


def canonical(moves, origin):
    """玉を原点とする移動座標。左右反転を同一視し、成・駒打ちは保持する。"""
    variants = []
    for sign in (1, -1):
        def relative(text):
            x, y = square(text)
            return (sign * (x - origin[0]), y - origin[1])
        variants.append(tuple(
            ("drop", move[0], *relative(move[2:4])) if move[1] == "*" else
            ("move", *relative(move[:2]), *relative(move[2:4]), move.endswith("+"))
            for move in moves))
    return min(variants)


@dataclass(frozen=True)
class Problem:
    sfen: str
    pv: tuple

    @classmethod
    def parse(cls, line):
        sfen, moves = line.strip().split(" moves ", 1)
        pv = tuple(moves.split())
        if not pv or len(pv) % 2 == 0 or any(not MOVE.fullmatch(m) for m in pv):
            raise ValueError("USI手順が不正です")
        board_from_sfen(sfen)
        return cls(sfen, pv)

    @property
    def line(self):
        return self.sfen + " moves " + " ".join(self.pv)

    @property
    def geometry(self):
        board = board_from_sfen(self.sfen)
        king = next(sq for sq, piece in board.items() if piece == "k")
        return canonical(self.pv, king)

    def keys(self):
        board = board_from_sfen(self.sfen)
        king = next(sq for sq, piece in board.items() if piece == "k")
        n = len(self.pv)
        # 最終手の選択は採択済みの同じ問題を別作にしない。
        keys = [(n, "prefix", canonical(self.pv[:-1], king)),
                (n, "absolute", canonical(self.pv[:-1], (5, 0)))]
        if n >= 9:
            # 初手の導入・玉の初期位置・最終手だけが違う長手数の類似作を束ねる。
            for move in self.pv[:2]:
                dest = square(move[2:4])
                if move[1] == "*":
                    piece = move[0]
                else:
                    piece = board.pop(square(move[:2]))
                    if piece == "k":
                        king = dest
                    if move.endswith("+"):
                        piece = "+" + piece
                board[dest] = piece
            core = canonical(self.pv[2:-1], king)
            # 中間手順の1手だけ違うものも同じ系統として採択しない。
            # 連鎖的なクラスタ統合はせず、採択済みの代表作と直接比較する。
            reflected = tuple((m[0], m[1], -m[2], m[3]) if m[0] == "drop" else
                              (m[0], -m[1], m[2], -m[3], m[4], m[5]) for m in core)
            keys.extend((n, "core", i, min(core[:i] + (None,) + core[i + 1:],
                                           reflected[:i] + (None,) + reflected[i + 1:]))
                        for i in range(len(core)))
        return keys


class DiversityIndex:
    """全ワーカー共通で使う。トリミング前後の双方で検査する。"""
    def __init__(self):
        self.seen = {}
        self.problems = []

    def conflict(self, problem):
        return next((self.seen[k] for k in problem.keys() if k in self.seen), None)

    def add(self, problem):
        if self.conflict(problem) is not None:
            return False
        number = len(self.problems)
        self.problems.append(problem)
        for key in problem.keys():
            self.seen[key] = number
        return True


def select(problems):
    index = DiversityIndex()
    for problem in problems:
        index.add(problem)
    return index


def spread(problems, window=10):
    """直近window題と手順座標がなるべく重ならない、再現可能な出題順。"""
    remaining = sorted(problems, key=lambda p: p.line)
    if not remaining:
        return []
    geometries = {p: p.geometry for p in remaining}
    result = [remaining.pop(0)]
    while remaining:
        recent = [geometries[p] for p in result[-window:]]
        def distance(problem):
            g = geometries[problem]
            return min(sum(a != b for a, b in zip(g, r)) for r in recent)
        best = max(range(len(remaining)), key=lambda i: distance(remaining[i]))
        result.append(remaining.pop(best))
    return result


def metrics(problems):
    counts = Counter(p.pv for p in problems)
    return {"positions": len(problems), "distinct_usi_lines": len(counts),
            "largest_identical_usi_group": max(counts.values(), default=0),
            "accepted_by_diversity_filter": len(select(problems).problems)}
