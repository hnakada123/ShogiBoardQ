"""Integration tests for the phase 1 tools (CLI backed) through a real MCP stdio session."""

from __future__ import annotations

import asyncio
import os
from pathlib import Path

import pytest

from conftest import FIXTURES, mcp_session, result_data

pytestmark = pytest.mark.anyio

UNIQUE_1PLY = "7nk/7nn/9/9/9/9/9/9/9 b N 1"
MATE_3PLY = "7k1/9/6L+S1/9/9/9/9/9/9 b 2r2b4g3s4n3l18p 1"


async def _call(session, tool_name, **arguments):
    return result_data(await session.call_tool(tool_name, arguments))


async def test_list_tools_and_resources(server_env):
    async with mcp_session(server_env) as session:
        tools = await session.list_tools()
        names = {t.name for t in tools.tools}
        assert {"convert_kifu", "validate_sfen", "list_engines", "analyze_position", "generate_tsume",
                "verify_tsume", "render_board_image", "get_app_state", "trigger_action"} <= names
        for tool in tools.tools:
            assert tool.description and tool.inputSchema["type"] == "object"
            assert tool.annotations is not None
        resources = await session.list_resources()
        assert {str(r.uri) for r in resources.resources} == {
            "shogiboardq://position/current", "shogiboardq://kifu/current", "shogiboardq://engines"}


async def test_validate_sfen(server_env):
    async with mcp_session(server_env) as session:
        text, data, is_error = await _call(session, "validate_sfen", sfen="startpos")
        assert not is_error
        assert data["valid"] is True
        assert data["turn"] == "b"
        assert data["legal_move_count"] == 30
        assert "Valid" in text

        text, data, is_error = await _call(session, "validate_sfen", sfen="garbage")
        assert not is_error  # invalid SFEN is a normal result, not a tool error
        assert data["valid"] is False and data["errors"]
        assert "Invalid" in text


async def test_input_validation_is_reported_as_error(server_env):
    async with mcp_session(server_env) as session:
        text, data, is_error = await _call(session, "validate_sfen")  # missing required argument
        assert is_error
        assert "sfen" in text


async def test_convert_kifu_from_file_and_text(server_env, tmp_path):
    async with mcp_session(server_env) as session:
        text, data, is_error = await _call(session, "convert_kifu", input_path=str(FIXTURES / "test_basic.kif"), output_format="usi")
        assert not is_error, text
        assert data["ply_count"] == 7
        assert data["usi_moves"][:2] == ["7g7f", "3c3d"]
        assert data["text"].startswith("position startpos moves 7g7f 3c3d")
        assert data["truncated"] is False

        csa_text = "V2.2\nN+Sente\nN-Gote\nPI\n+\n+7776FU\n-3334FU\n%TORYO\n"
        out = tmp_path / "converted.kif"
        text, data, is_error = await _call(session, "convert_kifu", text=csa_text, output_format="kif", output_path=str(out))
        assert not is_error, text
        assert data["input_format"] == "csa"
        assert data["output_path"] == str(out)
        assert out.exists()

        # existing output without overwrite -> error
        text, data, is_error = await _call(session, "convert_kifu", text=csa_text, output_format="kif", output_path=str(out))
        assert is_error and "overwrite" in text

        # relative output path -> error from the path policy
        text, data, is_error = await _call(session, "convert_kifu", text=csa_text, output_format="kif", output_path="relative.kif")
        assert is_error and "absolute" in text

        text, data, is_error = await _call(session, "convert_kifu", text="position startpos moves 7g7f 3c3d 2g2f", output_format="sfen", max_chars=1000)
        assert not is_error
        assert len(data["sfens"]) == 4


async def test_list_engines_and_resource(server_env):
    async with mcp_session(server_env) as session:
        text, data, is_error = await _call(session, "list_engines")
        assert not is_error
        names = {e["name"] for e in data["engines"]}
        if os.environ.get("SHOGIBOARDQ_TEST_USI_ENGINE"):
            assert "TestUsi" in names
        resource = await session.read_resource("shogiboardq://engines")  # type: ignore[arg-type]
        assert resource.contents and resource.contents[0].mimeType == "application/json"


async def test_render_board_image(server_env, tmp_path):
    async with mcp_session(server_env) as session:
        out = tmp_path / "board.png"
        text, data, is_error = await _call(session, "render_board_image", sfen="startpos", output_path=str(out), last_move="7g7f", square_size=30)
        assert not is_error, text
        assert Path(data["output_path"]) == out and out.stat().st_size > 1000
        assert data["width"] > 270 and data["height"] > 270


async def test_analysis_job(server_env):
    if not os.environ.get("SHOGIBOARDQ_TEST_USI_ENGINE"):
        pytest.skip("SHOGIBOARDQ_TEST_USI_ENGINE is not set")
    async with mcp_session(server_env) as session:
        text, data, is_error = await _call(session, "analyze_position", engine="TestUsi", sfen="startpos", moves=["7g7f"], seconds=2, multipv=2)
        assert not is_error, text
        job_id = data["job_id"]
        assert data["state"] == "running"

        for _ in range(60):
            text, status, is_error = await _call(session, "analysis_result", job_id=job_id, max_pv_moves=3)
            assert not is_error, text
            if status["state"] != "running":
                break
            await asyncio.sleep(0.25)
        assert status["state"] == "finished", text
        assert status["bestmove"]
        assert status["lines"] and all(len(line["pv"]) <= 3 for line in status["lines"])
        assert status["partial"] is False

        text, jobs, _ = await _call(session, "list_jobs")
        assert any(j["job_id"] == job_id for j in jobs["jobs"])

        text, data, is_error = await _call(session, "analyze_position", engine="NoSuchEngine", seconds=1)
        assert is_error and "Unknown engine" in text


async def test_mate_and_verify(server_env):
    if not os.environ.get("SHOGIBOARDQ_TEST_MATE_ENGINE"):
        pytest.skip("SHOGIBOARDQ_TEST_MATE_ENGINE is not set")
    async with mcp_session(server_env) as session:
        text, data, is_error = await _call(session, "search_mate", engine="TestMate", sfen=MATE_3PLY, seconds=5)
        assert not is_error, text
        job_id = data["job_id"]
        for _ in range(80):
            text, status, is_error = await _call(session, "mate_status", job_id=job_id)
            assert not is_error, text
            if status["state"] != "running":
                break
            await asyncio.sleep(0.25)
        assert status["state"] == "finished", text
        assert status["status"] == "mate"
        assert status["pv"] == ["3c3b+", "2a1a", "3b2b"]

        text, data, is_error = await _call(session, "verify_tsume", engine="TestMate", sfen=UNIQUE_1PLY, target_moves=1, timeout_seconds=10)
        assert not is_error, text
        assert data["status"] == "unique" and data["pv"] == ["N*2c"]

        text, data, is_error = await _call(session, "verify_tsume", engine="TestMate", sfen=MATE_3PLY, target_moves=3,
                                           timeout_seconds=10, allow_final_move_alternatives=False)
        assert not is_error, text
        assert data["status"] == "multiple"


async def test_generate_tsume_and_stop(server_env):
    if not os.environ.get("SHOGIBOARDQ_TEST_MATE_ENGINE"):
        pytest.skip("SHOGIBOARDQ_TEST_MATE_ENGINE is not set")
    async with mcp_session(server_env) as session:
        text, data, is_error = await _call(session, "generate_tsume", engine="TestMate", target_moves=1, max_positions=1, timeout_ms=2000)
        assert not is_error, text
        job_id = data["job_id"]
        for _ in range(240):
            text, status, is_error = await _call(session, "tsume_generation_status", job_id=job_id)
            assert not is_error, text
            if status["state"] != "running":
                break
            await asyncio.sleep(0.5)
        assert status["state"] == "finished", text
        assert status["found"] == 1 and len(status["positions"]) == 1
        assert status["positions"][0]["pv"]

        # start a longer job and stop it
        text, data, is_error = await _call(session, "generate_tsume", engine="TestMate", target_moves=7, max_positions=50, timeout_ms=2000)
        assert not is_error, text
        job_id = data["job_id"]
        await asyncio.sleep(1.0)
        text, status, is_error = await _call(session, "stop_tsume_generation", job_id=job_id)
        assert not is_error, text
        assert status["state"] == "stopped"
        text, data, is_error = await _call(session, "cancel_job", job_id=job_id)
        assert not is_error and data["state"] == "stopped"

        text, data, is_error = await _call(session, "generate_tsume", engine="TestMate", target_moves=2)
        assert is_error and "odd" in text
