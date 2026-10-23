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

# One-shot commands running at once (verify_tsume starts an engine). Jobs have their own limit (jobs.py).
MAX_CONCURRENT_COMMANDS = 4
_slots: asyncio.Semaphore | None = None
_slots_loop: asyncio.AbstractEventLoop | None = None
# The largest JSON result read from a one-shot command, and the stderr kept for error messages.
MAX_STDOUT_BYTES = 64 * 1024 * 1024
MAX_STDERR_BYTES = 64 * 1024


class _OutputTooLarge(Exception):
    pass


def _command_slots() -> asyncio.Semaphore:
    """The limit of the running event loop (a semaphore must not be shared between loops)."""
    global _slots, _slots_loop
    loop = asyncio.get_running_loop()
    if _slots is None or _slots_loop is not loop:
        _slots, _slots_loop = asyncio.Semaphore(MAX_CONCURRENT_COMMANDS), loop
    return _slots


async def _read_limited(stream: asyncio.StreamReader, limit: int, keep_tail: bool = False) -> bytes:
    """Read a pipe to its end. Over the limit: raise, or with keep_tail keep only the last ``limit`` bytes."""
    data = bytearray()
    while True:
        chunk = await stream.read(65536)
        if not chunk:
            return bytes(data)
        data += chunk
        if len(data) > limit:
            if not keep_tail:
                raise _OutputTooLarge
            del data[:len(data) - limit]


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
    async with _command_slots():
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

        async def collect() -> tuple[bytes, bytes]:
            out, err = await asyncio.gather(_read_limited(proc.stdout, MAX_STDOUT_BYTES),
                                            _read_limited(proc.stderr, MAX_STDERR_BYTES, keep_tail=True))
            await proc.wait()
            return out, err

        try:
            stdout, stderr = await asyncio.wait_for(collect(), timeout=timeout)
        except asyncio.TimeoutError:
            await _kill(proc)
            raise ToolError("timeout", f"shogiboardq-cli {args[0]} did not finish within {timeout:.0f} s")
        except _OutputTooLarge:
            await _kill(proc)
            raise ToolError("cli_failed", f"shogiboardq-cli {args[0]} printed more than {MAX_STDOUT_BYTES} bytes")
        except BaseException:
            # The request was cancelled (or the server stops): do not leave the CLI and its engine running.
            await _kill(proc)
            raise

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
        hint = error.get("hint")
        raise ToolError(error.get("code", "cli_failed"), error.get("message", "unknown error"),
                        {"hint": hint} if hint else None)
    return result


async def _kill(proc: asyncio.subprocess.Process) -> None:
    if proc.returncode is None:
        try:
            proc.kill()
        except ProcessLookupError:
            pass
        await asyncio.shield(proc.wait())


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
