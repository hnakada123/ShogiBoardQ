#!/usr/bin/env python3
"""Let a USI engine continue the sample game record (how full.usi was made).

usage: continue_game.py <engine executable> <last ply> [movetime ms]

Reads first50.usi (moves 1-50, read from the screenshots), lets the engine play until <last ply>
(103 for the sample) and writes the result to full.usi. The sample used YaneuraOu NNUE 9.10git
with its opening book off and 300 ms per move.
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ENGINE = os.path.abspath(sys.argv[1])
LAST = int(sys.argv[2])
MOVETIME = int(sys.argv[3]) if len(sys.argv) > 3 else 300

first = open(os.path.join(HERE, "first50.usi")).read().split()
moves = first[first.index("moves") + 1:]
engine = subprocess.Popen([ENGINE], cwd=os.path.dirname(ENGINE), stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                          text=True, bufsize=1)


def send(line):
    engine.stdin.write(line + "\n")
    engine.stdin.flush()


def wait(prefix):
    while True:
        line = engine.stdout.readline()
        if not line:
            sys.exit("engine exited")
        if line.startswith(prefix):
            return line.strip()


send("usi")
wait("usiok")
send("setoption name BookFile value no_book")
send("isready")
wait("readyok")
send("usinewgame")
while len(moves) < LAST:
    send("position startpos moves " + " ".join(moves))
    send(f"go movetime {MOVETIME}")
    best = wait("bestmove").split()[1]
    if best in ("resign", "win"):
        print("the engine ended the game at ply", len(moves), best)
        break
    moves.append(best)
send("quit")
with open(os.path.join(HERE, "full.usi"), "w") as f:
    f.write("position startpos moves " + " ".join(moves) + "\n")
print(len(moves), "moves")
