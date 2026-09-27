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


async def test_tsume_board_clicks(app_env, tmp_path):
    """Exercise real dialog mouse input, promotion, flipped hand drops and modal guards through MCP."""
    env = dict(app_env)
    config = tmp_path / "config" / "ShogiBoardQ"
    config.mkdir(parents=True)
    (config / "ShogiBoardQ.ini").write_text(
        "[TsumeCollection]\n"
        f"file={FIXTURES / 'tsume_positions_with_moves.sfen'}\nengine=@hayanagi\n"
        "[TsumePlay]\nsize=@Size(950 1180)\nsquareSize=65\n", encoding="utf-8")
    env["XDG_CONFIG_HOME"] = str(config.parent)
    env["XDG_DATA_HOME"] = str(tmp_path / "data")
    env["XDG_CACHE_HOME"] = str(tmp_path / "cache")
    env["SHOGIBOARDQ_AUTOMATION_SOCKET"] = str(tmp_path / "click.sock")
    async with mcp_session(env) as session:
        async def call(tool, **args):
            text, data, error = await _call(session, tool, **args)
            assert not error, text
            return data

        async def wait_ready():
            for _ in range(100):
                widgets = await call("get_widget_text", dialog="tsumePlayDialog", widget="tsumeStatus")
                if "あなたの手番" in widgets["widgets"][0]["text"]:
                    return widgets["widgets"][0]["text"]
                await asyncio.sleep(0.05)
            pytest.fail("Tsume dialog did not become ready")

        async def board():
            data = await call("get_widget_text", dialog="tsumePlayDialog", widget="tsumeBoard")
            return data["widgets"][0]

        async def layout():
            data = await call("get_widget_text", dialog="tsumePlayDialog")
            return {widget["object_name"]: widget["geometry"] for widget in data["widgets"]}

        async def button(widget):
            return await call("click_dialog_button", dialog="tsumePlayDialog", widget=widget)

        async def square(file, rank, **kwargs):
            return await call("click_board_square", target="tsumePlayDialog", file=file, rank=rank, **kwargs)

        names = {tool.name for tool in (await session.list_tools()).tools}
        assert {"click_board_square", "click_dialog_button"} <= names
        await call("trigger_action", name="actionTsumePlay")
        await asyncio.sleep(0.5)
        # Opening a modal dialog must prevent clicks reaching the main board behind it.
        text, _, error = await _call(session, "click_board_square", file=7, rank=7)
        assert error and "blocked" in text
        for file, rank in [(0, 1), (12, 1), (1, 10), (10, 9), (11, 1), (None, 1)]:
            _, _, error = await _call(session, "click_board_square", file=file, rank=rank)
            assert error

        for index in (0, 1):
            await call("click_dialog_button", dialog="tsumeCollectionDialog", widget="tsumeProblemCard", index=index)
            await asyncio.sleep(0.1)
            await wait_ready()
            if index == 1:
                await button("tsumeFlipBoard")
                await asyncio.sleep(0.05)
            initial = await board()
            fixed_names = ("tsumeBoard", "tsumePreviousProblem", "tsumeNextProblem", "tsumeTimeLimit",
                           "tsumeReduceBoard", "tsumeEnlargeBoard", "tsumeFlipBoard", "tsumeStatus",
                           "tsumePlayFontDecrease", "tsumePlayFontIncrease", "tsumeBackToCollection")
            initial_layout = await layout()
            assert initial["flipped"] is (index == 1)
            source = (3, 3) if index == 0 else (10, 5)  # ３三飛 or Black's gold in hand
            destination = (5, 3) if index == 0 else (9, 7)
            await square(*source)
            await square(*source, button="right")  # cancel selection
            assert (await board())["board_sfen"] == initial["board_sfen"]
            await square(*source)
            await square(*destination)
            if index == 0:
                await asyncio.sleep(0.1)
                text, _, error = await _call(session, "click_board_square", target="tsumePlayDialog", file=1, rank=1)
                assert error and "blocked" in text
                await call("click_dialog_button", dialog="成りの選択", text="成る")
            await asyncio.sleep(0.1)
            assert "残り3手" in await wait_ready()
            moved = await board()
            assert moved["board_sfen"] != initial["board_sfen"]
            assert moved["geometry"] == initial["geometry"]
            shot = await call("capture_screenshot", target="tsumePlayDialog", output_dir=str(tmp_path))
            assert Path(shot["path"]).exists()
            await button("tsumeUndo")
            await wait_ready()
            assert (await board())["board_sfen"] == initial["board_sfen"]
            await button("tsumeShowSolution")
            await asyncio.sleep(0.2)
            solution = await board()
            assert solution["geometry"] == initial["geometry"]
            await square(*source)
            await square(*destination)
            await asyncio.sleep(0.05)
            assert (await board())["board_sfen"] == solution["board_sfen"]
            # Hidden play buttons cannot be invoked while reviewing the solution.
            _, _, error = await _call(session, "click_dialog_button", dialog="tsumePlayDialog", widget="tsumeRestart")
            assert error
            await button("tsumeReturnToGame")
            assert (await board())["board_sfen"] == initial["board_sfen"]
            # Repeat the actual MCP button clicks; common controls and the two mode buttons
            # must keep identical rectangles, including after the solution has been cached.
            for _ in range(4):
                for widget, active in (("tsumeShowSolution", "tsumeReturnToGame"),
                                       ("tsumeReturnToGame", "tsumeShowSolution")):
                    await button(widget)
                    await asyncio.sleep(0.05)
                    current_layout = await layout()
                    assert current_layout[active] == initial_layout["tsumeShowSolution"]
                    assert {name: current_layout[name] for name in fixed_names} == {
                        name: initial_layout[name] for name in fixed_names}
            assert (await board())["board_sfen"] == initial["board_sfen"]
            await call("close_dialog", dialog="tsumePlayDialog")
            await asyncio.sleep(0.1)
        await call("close_dialog", dialog="tsumeCollectionDialog")
