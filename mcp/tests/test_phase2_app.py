"""Integration tests for the phase 2 tools: the server launches ShogiBoardQ --automation (offscreen)."""

from __future__ import annotations

import asyncio
import json
import os
import signal
from pathlib import Path

import pytest

from conftest import FIXTURES, mcp_session, result_data

pytestmark = pytest.mark.anyio


async def _call(session, tool_name, **arguments):
    return result_data(await session.call_tool(tool_name, arguments))


@pytest.fixture(scope="module")
def app_env(server_env, app_path, tmp_path_factory):
    env = dict(server_env)
    env["SHOGIBOARDQ_EXECUTABLE"] = str(app_path)
    env["SHOGIBOARDQ_QUIT_APP_ON_EXIT"] = "1"
    yield env
    # Safety net: the server quits the app it launched; kill it if that failed.
    endpoint = Path(env["XDG_CONFIG_HOME"]) / "ShogiBoardQ" / "automation-endpoint.json"
    if endpoint.exists():
        try:
            pid = int(json.loads(endpoint.read_text(encoding="utf-8")).get("pid", 0))
            if pid > 0:
                os.kill(pid, signal.SIGTERM)
        except (OSError, ValueError):
            pass


async def test_app_round_trip(app_env, tmp_path):
    async with mcp_session(app_env) as session:
        text, state, is_error = await _call(session, "get_app_state")
        assert not is_error, text
        assert state["ui_state"] == "idle"
        assert state["current_ply"] == 0
        assert state["dirty"] is False

        text, pos, is_error = await _call(session, "get_position")
        assert not is_error, text
        assert pos["sfen"].startswith("lnsgkgsnl/1r5b1/ppppppppp")
        assert pos["moves"] == []

        text, loaded, is_error = await _call(session, "load_kifu", path=str(FIXTURES / "test_basic.kif"))
        assert not is_error, text
        assert loaded["total_plies"] == 7
        assert loaded["kifu_file"].endswith("test_basic.kif")

        text, kifu, is_error = await _call(session, "get_kifu", max_moves=3)
        assert not is_error, text
        assert kifu["total_plies"] == 7
        assert [m["usi"] for m in kifu["moves"]] == ["7g7f", "3c3d", "2g2f"]
        assert kifu["moves"][0]["text"].endswith("７六歩(77)")

        text, data, is_error = await _call(session, "goto_ply", ply=3)
        assert not is_error, text
        assert data["ply"] == 3
        text, pos, is_error = await _call(session, "get_position")
        assert pos["ply"] == 3 and pos["moves"] == ["7g7f", "3c3d", "2g2f"]

        text, data, is_error = await _call(session, "goto_ply", ply=99)
        assert is_error and "beyond" in text

        text, csa, is_error = await _call(session, "get_kifu", format="csa")
        assert not is_error, text
        assert "+7776FU" in csa["text"]

        resource = await session.read_resource("shogiboardq://position/current")  # type: ignore[arg-type]
        assert resource.contents[0].text == pos["sfen"]  # type: ignore[union-attr]
        resource = await session.read_resource("shogiboardq://kifu/current")  # type: ignore[arg-type]
        assert "７六歩" in resource.contents[0].text  # type: ignore[union-attr]

        out = tmp_path / "saved.csa"
        text, saved, is_error = await _call(session, "save_kifu", path=str(out))
        assert not is_error, text
        assert out.exists() and saved["format"] == "csa"
        text, saved, is_error = await _call(session, "save_kifu", path=str(out))
        assert is_error and "overwrite" in text

        text, data, is_error = await _call(session, "load_kifu", text="position startpos moves 7g7f 3c3d")
        assert not is_error, text
        assert data["total_plies"] == 2
        # the pasted record is unsaved, so replacing it needs discard_unsaved
        text, data, is_error = await _call(session, "set_position", sfen="7nk/7nn/9/9/9/9/9/9/9 b N 1")
        assert is_error and "unsaved" in text.lower()
        text, data, is_error = await _call(session, "set_position", sfen="7nk/7nn/9/9/9/9/9/9/9 b N 1", discard_unsaved=True)
        assert not is_error, text
        assert data["sfen"].startswith("7nk/7nn/9")
        text, data, is_error = await _call(session, "set_position", sfen="garbage", discard_unsaved=True)
        assert is_error and "Invalid SFEN" in text


async def test_actions_dialogs_and_screenshots(app_env, tmp_path):
    async with mcp_session(app_env) as session:
        text, actions, is_error = await _call(session, "list_actions")
        assert not is_error, text
        names = {a["name"] for a in actions["actions"]}
        assert {"actionFlipBoard", "actionVersionInfo", "actionAnalyzeKifu"} <= names
        assert "actionQuit" not in names

        text, data, is_error = await _call(session, "trigger_action", name="actionQuit")
        assert is_error and "not allowed" in text
        text, data, is_error = await _call(session, "trigger_action", name="actionDoesNotExist")
        assert is_error

        text, state, _ = await _call(session, "get_app_state")
        flipped_before = state["board_flipped"]
        text, data, is_error = await _call(session, "trigger_action", name="actionFlipBoard")
        assert not is_error and data["triggered"] is True
        await asyncio.sleep(0.3)
        text, state, _ = await _call(session, "get_app_state")
        assert state["board_flipped"] != flipped_before

        text, data, is_error = await _call(session, "trigger_action", name="actionVersionInfo")
        assert not is_error, text
        for _ in range(20):
            await asyncio.sleep(0.2)
            text, dialogs, is_error = await _call(session, "list_dialogs")
            assert not is_error, text
            if any(w["object_name"] == "VersionDialog" for w in dialogs["windows"]):
                break
        assert any(w["object_name"] == "VersionDialog" for w in dialogs["windows"]), text

        text, widgets, is_error = await _call(session, "get_widget_text", dialog="VersionDialog")
        assert not is_error, text
        assert any("Version" in (w.get("text") or "") for w in widgets["widgets"])

        text, shot, is_error = await _call(session, "capture_screenshot", target="VersionDialog", output_dir=str(tmp_path))
        assert not is_error, text
        assert Path(shot["path"]).exists() and shot["width"] > 0

        text, data, is_error = await _call(session, "close_dialog", dialog="VersionDialog")
        assert not is_error, text
        await asyncio.sleep(0.3)
        text, dialogs, _ = await _call(session, "list_dialogs")
        assert not any(w["object_name"] == "VersionDialog" for w in dialogs["windows"])

        text, shot, is_error = await _call(session, "capture_screenshot")
        assert not is_error, text
        assert Path(shot["path"]).exists()

        text, widgets, is_error = await _call(session, "get_widget_text", max_rows=2)
        assert not is_error and widgets["widgets"]
