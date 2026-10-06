#!/usr/bin/env python3
"""Call one automation JSON-RPC method of a running ``ShogiBoardQ --automation``.

usage: rpc.py <socket path | instance name> <method> [json params]

An instance name is resolved to ``$SBQ_SOCKET_DIR/<name>.sock`` (default ``/tmp/sbq-<uid>``),
the socket that launch.sh gives to the app.
"""
import json
import os
import socket
import sys


def socket_path(name_or_path: str) -> str:
    if name_or_path.startswith("/"):
        return name_or_path
    return os.path.join(os.environ.get("SBQ_SOCKET_DIR", f"/tmp/sbq-{os.getuid()}"), name_or_path + ".sock")


def rpc(sock: str, method: str, params: dict | None = None, timeout: float = 120):
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(timeout)
    s.connect(socket_path(sock))
    s.sendall((json.dumps({"jsonrpc": "2.0", "id": 1, "method": method, "params": params or {}}) + "\n").encode())
    buf = b""
    while not buf.endswith(b"\n"):
        chunk = s.recv(1 << 20)
        if not chunk:
            break
        buf += chunk
    return json.loads(buf)


if __name__ == "__main__":
    params = json.loads(sys.argv[3]) if len(sys.argv) > 3 else None
    print(json.dumps(rpc(sys.argv[1], sys.argv[2], params), ensure_ascii=False))
