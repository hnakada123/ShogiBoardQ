"""Locations and limits of the running ShogiBoardQ, read from ``shogiboardq-cli environment``.

The CLI computes the automation socket, the endpoint file and the message limit exactly as the
application does (Qt's standard paths, ``SHOGIBOARDQ_CONFIG_HOME`` and friends), and lists the error
kinds shared by the CLI, the automation API and this server. Python reads it once per set of
environment variables instead of copying the values.
"""

from __future__ import annotations

import functools
import json
import os
import subprocess

from .cli_backend import cli_env, require_cli
from .errors import ToolError

# Variables that change what the application computes; a change re-reads the environment.
_RELEVANT = ("SHOGIBOARDQ_CONFIG_HOME", "SHOGIBOARDQ_AUTOMATION_SOCKET", "XDG_CONFIG_HOME", "XDG_RUNTIME_DIR",
             "HOME", "LOCALAPPDATA", "USERNAME", "SHOGIBOARDQ_CLI", "SHOGIBOARDQ_EXECUTABLE")


def environment() -> dict:
    """The JSON printed by ``shogiboardq-cli environment`` for the current environment variables."""
    return _load(tuple(os.environ.get(name, "") for name in _RELEVANT))


@functools.lru_cache(maxsize=8)
def _load(_key: tuple[str, ...]) -> dict:
    cli = require_cli()
    try:
        out = subprocess.run([str(cli), "environment"], capture_output=True, text=True, timeout=30,
                             env=cli_env(), check=False)
    except (OSError, subprocess.TimeoutExpired) as exc:
        raise ToolError("cli_failed", f"shogiboardq-cli environment failed: {exc}") from exc
    for line in reversed(out.stdout.splitlines()):
        try:
            data = json.loads(line)
        except ValueError:
            continue
        if isinstance(data, dict) and data.get("ok"):
            return data
    raise ToolError("cli_failed", f"shogiboardq-cli environment exited with code {out.returncode} without a result")


def error_slug(rpc_code: int) -> str:
    """The slug of a JSON-RPC error code (for errors that do not carry ``data.slug``)."""
    try:
        kinds = environment().get("error_kinds", [])
    except ToolError:
        kinds = []
    return next((k["slug"] for k in kinds if k.get("rpc_code") == rpc_code), "app_error")
