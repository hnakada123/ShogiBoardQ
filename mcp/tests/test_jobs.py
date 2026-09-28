"""Job cancellation during process startup and partial results after engine failure."""

from __future__ import annotations

import asyncio
import sys

import pytest

from shogiboardq_mcp import jobs
from shogiboardq_mcp.errors import ToolError
from shogiboardq_mcp.handlers_kifu_analysis import KifuAnalysisTools

pytestmark = pytest.mark.anyio


def step(ply, code):
    return {"ply": ply, "sfen": f"9/9/9/9/9/9/9/9/9 {'b' if ply % 2 == 0 else 'w'} - 1",
            "played_move": None, "args": ["-u", "-c", code]}


@pytest.mark.parametrize("sequence", [False, True])
async def test_cancel_during_spawn_reserves_slot_and_reaps_process(monkeypatch, sequence):
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
    steps = [step(0, code), step(1, code)]
    try:
        job = (await manager.start_sequence(steps, {"total_positions": 2}) if sequence
               else await manager.start("analysis", steps[0]["args"], {}))
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
        if sequence:
            assert len(job.data["positions"]) == 1 and job.data["positions"][0]["partial"]
    finally:
        release.set()
        await manager.shutdown()


async def test_sequence_failure_preserves_previous_scores_and_pagination(monkeypatch):
    monkeypatch.setattr(jobs, "require_cli", lambda: sys.executable)
    manager = jobs.JobManager()
    # The error is delayed so start_sequence can return the job ID first.
    success = 'print(\'{"event":"result","bestmove":"7g7f","lines":[{"score_cp":42,"pv":["7g7f","3c3d"]}]}\')'
    failure = 'import time; time.sleep(0.7); print(\'{"event":"error","message":"engine disconnected"}\')'
    job = await manager.start_sequence([step(1, success), step(2, failure)], {"total_positions": 2})
    try:
        await asyncio.wait_for(job.task, 5)
        assert job.state == "failed" and "engine disconnected" in job.error
        _, status = await KifuAnalysisTools(manager, None).status({"job_id": job.id, "max_pv_moves": 1})
        assert status["partial"] and status["completed"] == 1
        assert status["positions"][0]["score_cp_black"] == -42
        assert status["positions"][0]["lines"][0]["pv"] == ["7g7f"]
        _, empty_page = await KifuAnalysisTools(manager, None).status({"job_id": job.id, "offset": 1})
        assert empty_page["positions"] == [] and not empty_page["truncated"]
        assert not job.is_active() and job.process is None
    finally:
        await manager.shutdown()
