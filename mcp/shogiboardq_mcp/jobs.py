"""In-process job manager for long-running CLI commands.

Each job wraps one ``shogiboardq-cli`` subprocess that streams JSON Lines.
The manager keeps the latest state so status tools can answer immediately.

State machine::

    running -> finished   (CLI emitted result/finished)
    running -> failed     (CLI emitted error or exited unexpectedly)
    running -> stopping -> stopped   (stop/cancel requested)
"""

from __future__ import annotations

import asyncio
import json
import logging
import time
import uuid
from collections import deque
from dataclasses import dataclass, field
from typing import Any

from .cli_backend import cli_env, require_cli
from .errors import ToolError

log = logging.getLogger(__name__)

MAX_CONCURRENT_JOBS = 4
RETENTION_SECONDS = 30 * 60
STOP_GRACE_SECONDS = 6.0
KILL_GRACE_SECONDS = 3.0


@dataclass
class Job:
    id: str
    kind: str
    args: list[str]
    summary: dict[str, Any]
    state: str = "running"
    created_at: float = field(default_factory=time.monotonic)
    finished_at: float | None = None
    process: asyncio.subprocess.Process | None = None
    task: asyncio.Task | None = None
    events: deque = field(default_factory=lambda: deque(maxlen=200))
    stderr_tail: deque = field(default_factory=lambda: deque(maxlen=40))
    data: dict[str, Any] = field(default_factory=dict)
    result: dict[str, Any] | None = None
    error: str | None = None
    stop_requested: bool = False

    @property
    def elapsed_ms(self) -> int:
        end = self.finished_at or time.monotonic()
        return int((end - self.created_at) * 1000)

    def is_active(self) -> bool:
        return self.state in ("running", "stopping")

    def status_base(self) -> dict[str, Any]:
        base: dict[str, Any] = {
            "job_id": self.id,
            "kind": self.kind,
            "state": self.state,
            "elapsed_ms": self.elapsed_ms,
        }
        base.update(self.summary)
        if self.error:
            base["error"] = self.error
        return base


class JobManager:
    def __init__(self) -> None:
        self._jobs: dict[str, Job] = {}

    # ------------------------------------------------------------------ lifecycle
    async def start(self, kind: str, args: list[str], summary: dict[str, Any]) -> Job:
        self._purge()
        active = [j for j in self._jobs.values() if j.is_active()]
        if len(active) >= MAX_CONCURRENT_JOBS:
            raise ToolError(
                "too_many_jobs",
                f"{len(active)} jobs are already running (limit {MAX_CONCURRENT_JOBS}). "
                "Wait for one to finish or cancel it with cancel_job.",
            )
        cli = require_cli()
        job = Job(id=f"{kind}-{uuid.uuid4().hex[:8]}", kind=kind, args=args, summary=summary)
        try:
            job.process = await asyncio.create_subprocess_exec(
                str(cli), *args, "--stdin-control",
                stdin=asyncio.subprocess.PIPE,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
                env=cli_env(),
            )
        except OSError as exc:
            raise ToolError("cli_failed", f"Could not start {cli}: {exc}") from exc
        self._jobs[job.id] = job
        job.task = asyncio.create_task(self._pump(job))
        # Give the CLI a moment so argument/engine errors surface in the start call.
        try:
            await asyncio.wait_for(asyncio.shield(job.task), timeout=0.5)
        except asyncio.TimeoutError:
            pass
        if job.state == "failed" and not job.events:
            raise ToolError("cli_failed", job.error or "job failed to start")
        if job.state == "failed":
            raise ToolError("job_failed", job.error or "job failed to start")
        return job

    async def _pump(self, job: Job) -> None:
        proc = job.process
        assert proc is not None and proc.stdout is not None
        stderr_task = asyncio.create_task(self._drain_stderr(job))
        try:
            while True:
                line = await proc.stdout.readline()
                if not line:
                    break
                text = line.decode("utf-8", "replace").strip()
                if not text:
                    continue
                try:
                    event = json.loads(text)
                except ValueError:
                    log.debug("job %s: non-JSON line: %s", job.id, text)
                    continue
                if isinstance(event, dict):
                    self._apply(job, event)
            await proc.wait()
        finally:
            await stderr_task
            if job.state in ("running", "stopping"):
                if job.stop_requested:
                    job.state = "stopped"
                else:
                    job.state = "failed"
                    tail = " | ".join(list(job.stderr_tail)[-3:])
                    job.error = f"shogiboardq-cli exited with code {proc.returncode} before reporting a result" + (
                        f" ({tail})" if tail else ""
                    )
            job.finished_at = time.monotonic()

    async def _drain_stderr(self, job: Job) -> None:
        proc = job.process
        assert proc is not None and proc.stderr is not None
        while True:
            line = await proc.stderr.readline()
            if not line:
                return
            job.stderr_tail.append(line.decode("utf-8", "replace").rstrip())

    def _apply(self, job: Job, event: dict[str, Any]) -> None:
        job.events.append(event)
        name = event.get("event")
        data = job.data
        if name == "started":
            data["started"] = event
        elif name == "info":
            lines = data.setdefault("lines", {})
            lines[int(event.get("multipv", 1))] = event
            for key in ("depth", "nodes", "nps", "time_ms"):
                if key in event:
                    data[key] = event[key]
        elif name == "progress":
            data["progress"] = event
        elif name == "position":
            data.setdefault("positions", []).append(event)
        elif name == "result":
            job.result = event
            job.state = "stopped" if job.stop_requested else "finished"
        elif name == "finished":
            job.result = event
            job.state = "stopped" if (job.stop_requested or event.get("stopped")) else "finished"
        elif name == "error":
            job.error = event.get("message", "unknown error")
            job.state = "failed"

    # ------------------------------------------------------------------ queries
    def get(self, job_id: str) -> Job:
        job = self._jobs.get(job_id)
        if job is None:
            raise ToolError("job_not_found", f"Unknown job_id {job_id!r}. Jobs expire 30 minutes after they finish.")
        return job

    def list(self) -> list[Job]:
        self._purge()
        return sorted(self._jobs.values(), key=lambda j: j.created_at)

    def _purge(self) -> None:
        now = time.monotonic()
        for job_id, job in list(self._jobs.items()):
            if job.finished_at is not None and now - job.finished_at > RETENTION_SECONDS:
                del self._jobs[job_id]

    # ------------------------------------------------------------------ control
    async def stop(self, job_id: str) -> Job:
        job = self.get(job_id)
        if not job.is_active():
            return job
        job.stop_requested = True
        job.state = "stopping"
        proc = job.process
        assert proc is not None
        if proc.stdin is not None and not proc.stdin.is_closing():
            try:
                proc.stdin.write(b"stop\n")
                await proc.stdin.drain()
            except (BrokenPipeError, ConnectionResetError, OSError):
                pass
        try:
            await asyncio.wait_for(asyncio.shield(job.task), timeout=STOP_GRACE_SECONDS)
        except asyncio.TimeoutError:
            proc.terminate()
            try:
                await asyncio.wait_for(asyncio.shield(job.task), timeout=KILL_GRACE_SECONDS)
            except asyncio.TimeoutError:
                proc.kill()
                await job.task
        return job

    async def shutdown(self) -> None:
        for job in list(self._jobs.values()):
            if job.is_active():
                try:
                    await self.stop(job.id)
                except Exception:  # pragma: no cover - best effort
                    log.exception("failed to stop job %s", job.id)
