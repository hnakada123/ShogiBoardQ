"""Locate and run ``shogiboardq-cli``.

One-shot commands return a single JSON document on stdout. Long-running
commands (analyze, mate, generate-tsume) stream JSON Lines and are driven by
:mod:`shogiboardq_mcp.jobs` instead.
"""

from __future__ import annotations

import asyncio
import json
import os
import shutil
import sys
from pathlib import Path

from .errors import ToolError

_CLI_NAMES = ("shogiboardq-cli.exe", "shogiboardq-cli") if sys.platform.startswith("win") else ("shogiboardq-cli",)


def find_cli() -> Path | None:
    env = os.environ.get("SHOGIBOARDQ_CLI")
    if env:
        return Path(env)
    exe = os.environ.get("SHOGIBOARDQ_EXECUTABLE")
    if exe:
        for name in _CLI_NAMES:
            candidate = Path(exe).parent / name
            if candidate.exists():
                return candidate
    for name in _CLI_NAMES:
        found = shutil.which(name)
        if found:
            return Path(found)
    return None


def require_cli() -> Path:
    cli = find_cli()
    if cli is None or not cli.exists():
        raise ToolError(
            "cli_not_found",
            "shogiboardq-cli was not found. Set SHOGIBOARDQ_CLI to the executable built from "
            "ShogiBoardQ (build/shogiboardq-cli), or SHOGIBOARDQ_EXECUTABLE to the ShogiBoardQ binary "
            "that sits next to it.",
        )
    return cli


def cli_env() -> dict[str, str]:
    env = dict(os.environ)
    # The CLI renders boards with QWidget::grab; never require a display.
    env["QT_QPA_PLATFORM"] = "offscreen"
    return env


async def run_cli(args: list[str], timeout: float = 60.0) -> dict:
    """Run a one-shot CLI command and return its JSON result.

    Raises :class:`ToolError` when the CLI reports ``ok: false`` or fails to run.
    """
    cli = require_cli()
    try:
        proc = await asyncio.create_subprocess_exec(
            str(cli), *args,
            stdin=asyncio.subprocess.DEVNULL,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
            env=cli_env(),
        )
    except OSError as exc:
        raise ToolError("cli_failed", f"Could not start {cli}: {exc}") from exc
    try:
        stdout, stderr = await asyncio.wait_for(proc.communicate(), timeout=timeout)
    except asyncio.TimeoutError:
        proc.kill()
        await proc.wait()
        raise ToolError("timeout", f"shogiboardq-cli {args[0]} did not finish within {timeout:.0f} s")

    result = _last_json(stdout)
    if result is None:
        tail = stderr.decode("utf-8", "replace").strip().splitlines()[-5:]
        raise ToolError(
            "cli_failed",
            f"shogiboardq-cli {args[0]} exited with code {proc.returncode} without a JSON result. "
            + (" stderr: " + " | ".join(tail) if tail else ""),
        )
    if result.get("event") == "error":
        raise ToolError(result.get("code", "cli_failed"), result.get("message", "unknown error"))
    if not result.get("ok", True):
        error = result.get("error") or {}
        raise ToolError(error.get("code", "cli_failed"), error.get("message", "unknown error"))
    return result


def _last_json(stdout: bytes) -> dict | None:
    for line in reversed(stdout.decode("utf-8", "replace").splitlines()):
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            data = json.loads(line)
        except ValueError:
            continue
        if isinstance(data, dict):
            return data
    return None
