#!/usr/bin/env python3
"""Write the sample game record shown in the guide screenshots (Black: Suzuki, White: Yamada, 105 rows).

usage: mkkif.py [path to shogiboardq-cli]   (default: build/shogiboardq-cli of this repository)

Converts the 103 moves of full.usi to KIF with shogiboardq-cli, then adds the move times, player names and
comments that appear in the screenshots, and ends the record with a resignation as move 104. Writes, next to
this script:
  sample-<lang>.kif  the Consideration, Dock and Kifu Analysis pages (players Suzuki / Yamada)
  main-<lang>.kif    the main screenshot of the home page and README (players Black / White and so on)
for <lang> = ja, en, zh-cn and zh-tw.
"""
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(os.path.dirname(os.path.dirname(HERE)))
CLI = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, "build", "shogiboardq-cli")

# 1手ごとの消費時間（秒）。1〜14手は定跡で0、15〜50手は画像の棋譜欄から読み取った値、51手以降は5秒
SPENT = [0] * 14 + [6, 6, 6, 6, 7, 7, 6, 6, 6, 6, 5, 5, 6, 6, 5, 5, 6, 6,
                    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 5, 5]
SAMPLE_NAMES = {"ja": ("鈴木", "山田"), "en": ("Suzuki", "Yamada"), "zh-cn": ("铃木", "山田"), "zh-tw": ("鈴木", "山田")}
# ホームのメイン画像の対局者名（撮り直す前の画像に合わせた）
MAIN_NAMES = {"ja": ("先手", "後手"), "en": ("Black", "White"), "zh-cn": ("先手", "后手"),
              "zh-tw": ("Hayanagi 1.5.1", "Hayanagi 1.5.1")}
# 15手目と49手目のコメント。棋譜欄には書き出しだけが写る
COMMENTS = {
    "ja": {15: "先手は飛車先の歩を交換して、攻めの形を作る。", 49: "ここから後手が反撃に出る。"},
    "en": {15: "Black exchanges the rook-file pawn to build an attack.", 49: "White takes the initiative from here."},
    "zh-cn": {15: "先手交换飞车前的步，构筑攻势。", 49: "后手从这里开始反击。"},
    "zh-tw": {15: "先手交換飛車前的步，構築攻勢。", 49: "後手從這裡開始反擊。"},
}


def converted_moves():
    with tempfile.TemporaryDirectory() as tmp:
        out = os.path.join(tmp, "full.kif")
        subprocess.run([CLI, "convert-kifu", "--input", os.path.join(HERE, "full.usi"), "--input-format", "usi",
                        "--output-format", "kif", "--output", out], check=True, stdout=subprocess.DEVNULL)
        text = open(out, encoding="cp932").read()
    return [m.group(1) for m in re.finditer(r"^\s*\d+\s+(.*?)\s+\(", text, re.M)]


def write(path, black, white, comments, moves):
    out = ["#KIF version=2.0 encoding=UTF-8", "開始日時：2026/10/04 22:16:36", "手合割：平手",
           f"先手：{black}", f"後手：{white}", "手数----指手---------消費時間--"]
    total = [0, 0]
    for ply, move in enumerate(moves, 1):
        spent = SPENT[ply - 1] if ply <= len(SPENT) else 5
        total[(ply - 1) % 2] += spent
        t = total[(ply - 1) % 2]
        out.append(f"{ply:4d} {move:<12} ({spent // 60:2d}:{spent % 60:02d}/{t // 3600:02d}:{t // 60 % 60:02d}:{t % 60:02d})")
        if ply in comments:
            out.append("*" + comments[ply])
    out.append(f"{len(moves) + 1:4d} 投了")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")


def main():
    moves = converted_moves()
    if len(moves) != 103:
        sys.exit(f"expected 103 moves, got {len(moves)}")
    for lang in SAMPLE_NAMES:
        write(os.path.join(HERE, f"sample-{lang}.kif"), *SAMPLE_NAMES[lang], COMMENTS[lang], moves)
        write(os.path.join(HERE, f"main-{lang}.kif"), *MAIN_NAMES[lang], COMMENTS[lang], moves)
    print("wrote", ", ".join(f"{kind}-{lang}.kif" for kind in ("sample", "main") for lang in SAMPLE_NAMES))


if __name__ == "__main__":
    main()
