"""Job cancellation during process startup and partial results after engine failure."""

from __future__ import annotations

import asyncio
import sys

import pytest

from shogiboardq_mcp.server import with_defaults

from shogiboardq_mcp import jobs
from shogiboardq_mcp.errors import ToolError
from shogiboardq_mcp.handlers_kifu_analysis import KifuAnalysisTools

pytestmark = pytest.mark.anyio


def script(code):
    return ["-u", "-c", code]


async def test_cancel_during_spawn_reserves_slot_and_reaps_process(monkeypatch):
    monkeypatch.setattr(jobs, "require_cli", lambda: sys.executable)
    monkeypatch.setattr(jobs, "MAX_CONCURRENT_JOBS", 1)
    manager = jobs.JobManager()
    release = asyncio.Event()
    processes = []
    spawn = manager._spawn

    async def delayed_spawn(job):
        await release.wait()
        await spawn(job)
        processes.append(job.process)

    monkeypatch.setattr(manager, "_spawn", delayed_spawn)
    code = 'import sys; sys.stdin.readline(); print(\'{"event":"result","lines":[]}\')'
    try:
        job = await manager.start("analysis", script(code), {})
        assert job.process is None and job.state == "running"
        with pytest.raises(ToolError, match="already running"):
            await manager.start("analysis", [], {})
        stopping = asyncio.create_task(manager.stop(job.id))
        await asyncio.sleep(0)
        assert job.stop_requested
        release.set()
        await asyncio.wait_for(stopping, 5)
        assert job.state == "stopped"
        assert len(processes) == 1 and processes[0].returncode == 0
    finally:
        release.set()
        await manager.shutdown()


async def test_kifu_analysis_failure_preserves_previous_scores_and_pagination(monkeypatch):
    monkeypatch.setattr(jobs, "require_cli", lambda: sys.executable)
    manager = jobs.JobManager()
    # shogiboardq-cli analyze-kifu reports each position, then fails; the error is delayed so start returns first.
    code = ('import time; '
            'print(\'{"event":"started","total_positions":2}\'); '
            'print(\'{"event":"position","ply":1,"sfen":"9/9/9/9/9/9/9/9/9 w - 1","bestmove":"7g7f",'
            '"score_cp_black":-42,"partial":false,"lines":[{"score_cp":42,"pv":["7g7f","3c3d"]}]}\'); '
            'time.sleep(0.7); print(\'{"event":"error","code":"engine_error","message":"engine disconnected"}\')')
    job = await manager.start("kifu_analysis", script(code), {"total_positions": 2}, until="started")
    try:
        await asyncio.wait_for(job.task, 5)
        assert job.state == "failed" and "engine disconnected" in job.error
        _, status = await KifuAnalysisTools(manager, None).status(
            with_defaults("kifu_analysis_status", {"job_id": job.id, "max_pv_moves": 1}))
        assert status["partial"] and status["completed"] == 1
        assert status["positions"][0]["score_cp_black"] == -42
        assert status["positions"][0]["lines"][0]["pv"] == ["7g7f"]
        _, empty_page = await KifuAnalysisTools(manager, None).status(
            with_defaults("kifu_analysis_status", {"job_id": job.id, "offset": 1}))
        assert empty_page["positions"] == [] and not empty_page["truncated"]
        assert not job.is_active() and job.process.returncode is not None
    finally:
        await manager.shutdown()
