#!/usr/bin/env python3
"""Play the human (Black) side of a running ShogiBoardQ game.

Each human move is chosen by a separate Hayanagi process and entered with
board.click, so the GUI records it exactly like a mouse move.

usage: humanplay.py <socket path | instance name> <number of human moves> [movetime ms]

The engine is $SBQ_HAYANAGI (default: build/Hayanagi/hayanagi of this repository).
"""
import json
import os
import socket
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from rpc import socket_path  # noqa: E402

SOCK = socket_path(sys.argv[1])
COUNT = int(sys.argv[2])
MOVETIME = int(sys.argv[3]) if len(sys.argv) > 3 else 600
REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ENGINE = os.environ.get("SBQ_HAYANAGI", os.path.join(REPO, "build", "Hayanagi", "hayanagi"))
HAND_RANK = {"P": 1, "L": 2, "N": 3, "S": 4, "G": 5, "B": 6, "R": 7}
PROMOTE = {"成る", "Promote", "成"}
NO_PROMOTE = {"成らない", "Do not promote", "不成"}


def rpc(method, params=None):
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(120)
    s.connect(SOCK)
    s.sendall((json.dumps({"jsonrpc": "2.0", "id": 1, "method": method, "params": params or {}}) + "\n").encode())
    buf = b""
    while not buf.endswith(b"\n"):
        chunk = s.recv(1 << 20)
        if not chunk:
            break
        buf += chunk
    resp = json.loads(buf)
    if "error" in resp:
        raise RuntimeError(f"{method}: {resp['error']}")
    return resp["result"]


class Engine:
    def __init__(self):
        self.p = subprocess.Popen([ENGINE], cwd=os.path.dirname(ENGINE), stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, text=True, bufsize=1)
        self.send("usi")
        self.wait("usiok")
        self.send("isready")
        self.wait("readyok")

    def send(self, line):
        self.p.stdin.write(line + "\n")

    def wait(self, token):
        while True:
            line = self.p.stdout.readline().strip()
            if line.startswith(token):
                return line

    def best(self, sfen):
        self.send(f"position sfen {sfen}")
        self.send(f"go movetime {MOVETIME}")
        return self.wait("bestmove").split()[1]


def square(usi_sq):
    return int(usi_sq[0]), ord(usi_sq[1]) - ord("a") + 1


def click(file, rank):
    rpc("board.click", {"file": file, "rank": rank})
    time.sleep(0.35)


def answer_promotion(promote):
    for _ in range(5):
        dlg = [w for w in rpc("dialog.list")["windows"] if w["object_name"] == "PromoteDialog"]
        if dlg:
            texts = PROMOTE if promote else NO_PROMOTE
            for w in rpc("widget.text", {"dialog": dlg[0]["selector"]})["widgets"]:
                if w["class"] == "QPushButton" and w.get("text", "").replace("&", "") in texts:
                    rpc("dialog.clickButton", {"dialog": dlg[0]["selector"], "widget": w["selector"]})
                    return
        time.sleep(0.2)


def black_to_move():
    st = rpc("app.state")
    return st["ui_state"] == "game" and st["sfen"].split()[1] == "b"


engine = Engine()
for n in range(COUNT):
    for _ in range(600):
        st = rpc("app.state")
        if st["ui_state"] != "game":
            print("game is over")
            sys.exit(0)
        if st["sfen"].split()[1] == "b":
            break
        time.sleep(0.5)
    sfen = rpc("app.state")["sfen"]
    move = engine.best(sfen)
    if move in ("resign", "win"):
        print("engine says", move)
        break
    if "*" in move:
        click(10, HAND_RANK[move[0]])
        click(*square(move[2:4]))
    else:
        click(*square(move[0:2]))
        click(*square(move[2:4]))
        answer_promotion(move.endswith("+"))
    time.sleep(0.8)
    after = rpc("app.state")
    print(n + 1, move, "ply", after["current_ply"], flush=True)
engine.send("quit")
