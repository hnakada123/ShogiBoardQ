"""JSON-RPC 2.0 client for the ShogiBoardQ automation socket.

The application listens on a local socket (Unix domain socket or Windows named
pipe) when started with ``--automation``. Messages are newline delimited.
"""

from __future__ import annotations

import asyncio
import json
import logging
import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

from . import paths
from .errors import ToolError

log = logging.getLogger(__name__)

CALL_TIMEOUT = 30.0
LAUNCH_TIMEOUT = 30.0


class AppError(ToolError):
    """A JSON-RPC error returned by the application."""

    def __init__(self, rpc_code: int, message: str, data: dict | None = None):
        slug = {
            -32601: "method_not_found",
            -32602: "invalid_params",
            -32001: "not_allowed",
            -32002: "invalid_state",
            -32003: "file_error",
            -32004: "unsaved_changes",
            -32005: "not_found",
        }.get(rpc_code, "app_error")
        super().__init__(slug, message, data)
        self.rpc_code = rpc_code


class AppClient:
    def __init__(self) -> None:
        self._reader: asyncio.StreamReader | None = None
        self._writer: asyncio.StreamWriter | None = None
        self._lock = asyncio.Lock()
        self._next_id = 1
        self._launched: subprocess.Popen | None = None
        self._socket_path: str | None = None

    # ------------------------------------------------------------------ connection
    def is_connected(self) -> bool:
        return self._writer is not None and not self._writer.is_closing()

    def _candidate_sockets(self) -> list[str]:
        candidates: list[str] = []
        env = os.environ.get("SHOGIBOARDQ_AUTOMATION_SOCKET")
        if env:
            candidates.append(env)
        endpoint = paths.read_endpoint()
        if endpoint and isinstance(endpoint.get("socket"), str):
            candidates.append(endpoint["socket"])
        candidates.append(paths.default_socket_path())
        seen: set[str] = set()
        return [c for c in candidates if not (c in seen or seen.add(c))]

    async def _open(self, socket_path: str) -> tuple[asyncio.StreamReader, asyncio.StreamWriter]:
        if sys.platform.startswith("win"):
            loop = asyncio.get_running_loop()
            reader = asyncio.StreamReader()
            protocol = asyncio.StreamReaderProtocol(reader)
            transport, _ = await loop.create_pipe_connection(lambda: protocol, socket_path)  # type: ignore[attr-defined]
            writer = asyncio.StreamWriter(transport, protocol, reader, loop)
            return reader, writer
        return await asyncio.open_unix_connection(socket_path)

    async def connect(self) -> None:
        if self.is_connected():
            return
        errors: list[str] = []
        for candidate in self._candidate_sockets():
            try:
                self._reader, self._writer = await asyncio.wait_for(self._open(candidate), timeout=3.0)
                self._socket_path = candidate
                log.info("connected to ShogiBoardQ automation socket %s", candidate)
                return
            except (OSError, asyncio.TimeoutError) as exc:
                errors.append(f"{candidate}: {exc}")
        await self._launch_and_connect(errors)

    async def _launch_and_connect(self, errors: list[str]) -> None:
        exe = os.environ.get("SHOGIBOARDQ_EXECUTABLE")
        if not exe:
            raise ToolError(
                "app_unavailable",
                "ShogiBoardQ is not running with --automation and SHOGIBOARDQ_EXECUTABLE is not set. "
                "Start `ShogiBoardQ --automation` yourself, or set SHOGIBOARDQ_EXECUTABLE so the server "
                "can launch it. Tried: " + "; ".join(errors),
            )
        exe_path = Path(exe)
        if not exe_path.exists():
            raise ToolError("app_unavailable", f"SHOGIBOARDQ_EXECUTABLE does not exist: {exe}")
        if self._launched is not None and self._launched.poll() is None:
            socket_path = self._socket_path or paths.default_socket_path()
        else:
            socket_path = os.environ.get("SHOGIBOARDQ_AUTOMATION_SOCKET") or paths.default_socket_path()
            cmd = [str(exe_path), "--automation", "--automation-socket", socket_path]
            log.info("launching %s", " ".join(cmd))
            try:
                self._launched = subprocess.Popen(  # noqa: S603 - path comes from the operator's environment
                    cmd,
                    stdin=subprocess.DEVNULL,
                    stdout=subprocess.DEVNULL,
                    stderr=subprocess.DEVNULL,
                    start_new_session=not sys.platform.startswith("win"),
                )
            except OSError as exc:
                raise ToolError("app_unavailable", f"Could not start ShogiBoardQ: {exc}") from exc
        deadline = time.monotonic() + LAUNCH_TIMEOUT
        last_error = ""
        while time.monotonic() < deadline:
            if self._launched is not None and self._launched.poll() is not None:
                raise ToolError(
                    "app_unavailable",
                    f"ShogiBoardQ exited with code {self._launched.returncode} right after launch. "
                    "Run it manually with --automation to see the error.",
                )
            try:
                self._reader, self._writer = await asyncio.wait_for(self._open(socket_path), timeout=2.0)
                self._socket_path = socket_path
                log.info("connected to launched ShogiBoardQ at %s", socket_path)
                return
            except (OSError, asyncio.TimeoutError) as exc:
                last_error = str(exc)
                await asyncio.sleep(0.3)
        raise ToolError("app_unavailable", f"ShogiBoardQ did not open {socket_path} within {LAUNCH_TIMEOUT:.0f} s: {last_error}")

    async def close(self) -> None:
        if self._writer is not None:
            try:
                self._writer.close()
                await self._writer.wait_closed()
            except Exception:  # pragma: no cover - best effort
                pass
        self._reader = None
        self._writer = None

    async def shutdown(self) -> None:
        """Close the connection, quitting the app only if we launched it and the operator asked for it."""
        if self._launched is not None and os.environ.get("SHOGIBOARDQ_QUIT_APP_ON_EXIT") == "1":
            try:
                await self.call("app.quit", timeout=5.0)
            except Exception:  # pragma: no cover - best effort
                pass
        await self.close()

    # ------------------------------------------------------------------ calls
    async def call(self, method: str, params: dict[str, Any] | None = None, timeout: float = CALL_TIMEOUT) -> Any:
        async with self._lock:
            await self.connect()
            try:
                return await asyncio.wait_for(self._call_locked(method, params), timeout=timeout)
            except (ConnectionError, asyncio.IncompleteReadError, asyncio.TimeoutError, OSError) as exc:
                await self.close()
                if isinstance(exc, asyncio.TimeoutError):
                    raise ToolError(
                        "app_timeout",
                        f"ShogiBoardQ did not answer {method} within {timeout:.0f} s. A modal dialog may be open; "
                        "use list_dialogs / capture_screenshot to inspect it.",
                    ) from exc
                raise ToolError("app_disconnected", f"Connection to ShogiBoardQ was lost during {method}: {exc}") from exc

    async def _call_locked(self, method: str, params: dict[str, Any] | None) -> Any:
        assert self._reader is not None and self._writer is not None
        request_id = self._next_id
        self._next_id += 1
        message = {"jsonrpc": "2.0", "id": request_id, "method": method}
        if params:
            message["params"] = params
        self._writer.write((json.dumps(message, ensure_ascii=False) + "\n").encode("utf-8"))
        await self._writer.drain()
        while True:
            line = await self._reader.readline()
            if not line:
                raise ConnectionError("connection closed by ShogiBoardQ")
            try:
                response = json.loads(line.decode("utf-8"))
            except ValueError:
                log.warning("ignoring non-JSON line from app: %r", line[:200])
                continue
            if not isinstance(response, dict) or response.get("id") != request_id:
                log.debug("ignoring unrelated message: %r", response)
                continue
            if "error" in response:
                error = response["error"] or {}
                raise AppError(int(error.get("code", -32603)), str(error.get("message", "error")), error.get("data"))
            return response.get("result")
