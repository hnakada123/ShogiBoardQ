"""Operate the actual consideration UI through an MCP stdio session."""
from __future__ import annotations

import asyncio
import os
from pathlib import Path

import pytest

from conftest import FIXTURES, mcp_session, result_data

pytestmark = pytest.mark.anyio


@pytest.fixture
def consideration_env(server_env, app_path, tmp_path):
    engine = os.environ.get("SHOGIBOARDQ_TEST_USI_ENGINE")
    if not engine:
        pytest.skip("SHOGIBOARDQ_TEST_USI_ENGINE is required")
    env = dict(server_env)
    config = tmp_path / "config" / "ShogiBoardQ"
    config.mkdir(parents=True)
    (config / "ShogiBoardQ.ini").write_text(
        "[Engines]\nsize=2\n1\\name=TestUsi\n1\\path=" + engine
        + "\n2\\name=SecondUsi\n2\\path=" + engine
        + "\n[Consideration]\nengineIndex=1\nunlimitedTime=true\nbyoyomiSec=7\nmultiPV=3\n"
        + "[General]\nmainWindowSize=@Size(1400 1000)\n", encoding="utf-8")
    env.update(SHOGIBOARDQ_EXECUTABLE=str(app_path), SHOGIBOARDQ_QUIT_APP_ON_EXIT="1",
               XDG_CONFIG_HOME=str(config.parent), SHOGIBOARDQ_AUTOMATION_SOCKET=str(tmp_path / "app.sock"))
    return env


class UI:
    def __init__(self, session):
        self.session = session

    async def call(self, tool, **args):
        text, data, error = result_data(await self.session.call_tool(tool, args))
        assert not error, f"{tool} {args}: {text}"
        return data

    async def widget(self, name):
        return (await self.call("get_widget_text", widget=name))["widgets"][0]

    async def set(self, name, value):
        await self.call("set_widget_value", widget=name, value=value)

    async def click(self, name):
        await self.call("click_widget", widget=name)

    async def wait(self, name, key, expected, timeout=8):
        deadline = asyncio.get_running_loop().time() + timeout
        while True:
            widget = await self.widget(name)
            if widget.get(key) == expected:
                return widget
            assert asyncio.get_running_loop().time() < deadline, (name, key, expected, widget)
            await asyncio.sleep(0.1)

    async def wait_engine(self, name):
        deadline = asyncio.get_running_loop().time() + 10
        while True:
            state = await self.call("get_app_state")
            if state["engines"]["black"] == name:
                return
            assert asyncio.get_running_loop().time() < deadline, state
            await asyncio.sleep(0.1)

    async def board(self, **args):
        widgets = (await self.call("get_widget_text", **args))["widgets"]
        return next(w for w in widgets if "board_sfen" in w)

    async def open_pv(self):
        # Navigation replaces the model asynchronously. A queued click is discarded
        # if its persistent index is invalidated; reacquire the rows before retrying.
        deadline = asyncio.get_running_loop().time() + 8
        while asyncio.get_running_loop().time() < deadline:
            await self.wait("considerationView", "row_count", 3)
            await self.call("click_table_cell", widget="considerationView", row=0, column=4)
            for _ in range(10):
                await asyncio.sleep(0.1)
                dialogs = (await self.call("list_dialogs"))["windows"]
                matches = [w for w in dialogs if w["class"] == "PvBoardDialog"]
                if matches:
                    return matches[0]
        pytest.fail(f"PV dialog did not open: {await self.widget('considerationView')}")

    async def start(self, *, multipv=1, seconds=None):
        await self.call("get_app_state")
        await self.call("show_dock", widget="ConsiderationDock")
        await self.set("considerationMultiPV", multipv - 1)
        if seconds is None:
            await self.set("considerationUnlimited", True)
        else:
            await self.set("considerationTimed", True)
            await self.set("considerationSeconds", seconds)
        await self.click("considerationStartStop")
        await self.wait("considerationStartStop", "text", "検討中止")
        await self.wait("considerationView", "row_count", multipv)

    async def stop(self):
        await self.click("considerationStartStop")
        await self.wait("considerationStartStop", "text", "検討開始")


async def test_restores_settings(consideration_env):
    async with mcp_session(consideration_env) as session:
        ui = UI(session)
        await ui.call("get_app_state")
        assert (await ui.widget("considerationEngine"))["current_index"] == 1
        assert (await ui.widget("considerationUnlimited"))["checked"] is True
        assert (await ui.widget("considerationSeconds"))["value"] == 7
        assert (await ui.widget("considerationMultiPV"))["current_index"] == 2


async def test_changes_multipv_while_searching(consideration_env):
    async with mcp_session(consideration_env) as session:
        ui = UI(session)
        await ui.start()
        try:
            await ui.set("considerationMultiPV", 2)
            await ui.wait("considerationView", "row_count", 3)
            await ui.set("considerationMultiPV", 0)
            await ui.wait("considerationView", "row_count", 1)
        finally:
            await ui.stop()


async def test_changes_multipv_after_time_limit(consideration_env):
    async with mcp_session(consideration_env) as session:
        ui = UI(session)
        await ui.start(seconds=1)
        try:
            await asyncio.sleep(1.5)
            await ui.set("considerationMultiPV", 2)
            await ui.wait("considerationView", "row_count", 3)
        finally:
            await ui.stop()


async def test_stop_does_not_restore_arrows(consideration_env):
    async with mcp_session(consideration_env) as session:
        ui = UI(session)
        await ui.start(multipv=3)
        assert len((await ui.board())["arrows"]) == 3
        await ui.stop()
        await ui.set("considerationArrows", False)
        await ui.set("considerationArrows", True)
        assert (await ui.board())["arrows"] == []


async def test_navigation_pv_board_and_display(consideration_env, tmp_path):
    async with mcp_session(consideration_env) as session:
        ui = UI(session)
        await ui.call("load_kifu", path=str(FIXTURES / "test_basic.kif"))
        await ui.call("goto_ply", ply=0)
        await ui.start(multipv=3, seconds=1)
        try:
            for name in ("considerationUnlimited", "considerationTimed", "considerationSeconds"):
                assert (await ui.widget(name))["enabled"] is False
            await asyncio.sleep(1.5)
            elapsed = (await ui.widget("considerationElapsed"))["text"]
            await asyncio.sleep(0.3)
            assert (await ui.widget("considerationElapsed"))["text"] == elapsed
            # 停止時の PV なし info（time/nodes のみ）で最善手の行が空にならない
            rows = (await ui.widget("considerationView"))["rows"][1:]
            assert rows and all(row[5] for row in rows), rows
            for ply in (1, 3, 0, 2):
                pos = await ui.call("goto_ply", ply=ply)
                assert pos["ply"] == ply
                await ui.wait("considerationView", "row_count", 3)
                assert (await ui.board())["board_sfen"] == pos["sfen"].split()[0]
                dialog = await ui.open_pv()
                title = dialog["title"]
                board = await ui.board(dialog=title)
                assert board["board_sfen"] == pos["sfen"].split()[0]
                await ui.call("click_dialog_button", dialog=title, text="▶")
                assert (await ui.board(dialog=title))["board_sfen"] != board["board_sfen"]
                await ui.call("click_dialog_button", dialog=title, text="⏮")
                assert (await ui.board(dialog=title))["board_sfen"] == board["board_sfen"]
                await ui.call("close_dialog", dialog=title)
            await ui.set("considerationArrows", False)
            assert (await ui.board())["arrows"] == []
            await ui.set("considerationArrows", True)
            arrows = (await ui.board())["arrows"]
            assert [a["priority"] for a in arrows] == [1, 2, 3]
            await ui.call("trigger_action", name="actionFlipBoard")
            assert (await ui.board())["flipped"] is True
            assert (await ui.board())["arrows"] == arrows
            font = (await ui.widget("considerationView"))["font_point_size"]
            await ui.click("considerationFontIncrease")
            assert (await ui.widget("considerationView"))["font_point_size"] == font + 1
            await ui.click("considerationFontDecrease")
            assert (await ui.widget("considerationView"))["font_point_size"] == font
            shot = await ui.call("capture_screenshot", output_dir=str(tmp_path))
            assert Path(shot["path"]).exists()
            await ui.call("show_dock", widget="ThinkingDock")
            assert (await ui.widget("thinkingView1"))["row_count"] > 0
            await ui.call("show_dock", widget="ConsiderationDock")
        finally:
            await ui.stop()
        for name in ("considerationUnlimited", "considerationTimed", "considerationSeconds"):
            assert (await ui.widget(name))["enabled"] is True


async def test_switch_engine_and_restart(consideration_env):
    async with mcp_session(consideration_env) as session:
        ui = UI(session)
        await ui.start(multipv=3)
        try:
            await ui.set("considerationEngine", "TestUsi")
            await ui.wait_engine("TestUsi")
            await ui.wait("considerationView", "row_count", 3)
            assert (await ui.widget("considerationStartStop"))["text"] == "検討中止"
            assert (await ui.call("get_app_state"))["engines"]["black"] == "TestUsi"
            await ui.set("considerationEngine", "SecondUsi")
            await ui.wait_engine("SecondUsi")
            await ui.wait("considerationView", "row_count", 3)
            assert (await ui.call("get_app_state"))["engines"]["black"] == "SecondUsi"
        finally:
            await ui.stop()
        for _ in range(2):
            await ui.start(multipv=3)
            await ui.stop()
            assert (await ui.board())["arrows"] == []


async def test_saves_changed_settings_without_starting(consideration_env):
    async with mcp_session(consideration_env) as session:
        ui = UI(session)
        await ui.call("get_app_state")
        await ui.call("show_dock", widget="ConsiderationDock")
        await ui.set("considerationEngine", 0)
        await ui.set("considerationTimed", True)
        await ui.set("considerationSeconds", 4)
        # MultiPV must be saved even if no subsequent control change occurs.
        await ui.set("considerationMultiPV", 4)
    async with mcp_session(consideration_env) as session:
        ui = UI(session)
        await ui.call("get_app_state")
        assert (await ui.widget("considerationEngine"))["current_index"] == 0
        assert (await ui.widget("considerationTimed"))["checked"] is True
        assert (await ui.widget("considerationSeconds"))["value"] == 4
        assert (await ui.widget("considerationMultiPV"))["current_index"] == 4


async def test_widget_operations_reject_invalid_input(consideration_env):
    async with mcp_session(consideration_env) as session:
        ui = UI(session)
        await ui.call("get_app_state")
        await ui.call("show_dock", widget="ConsiderationDock")
        for tool, args in (
            ("click_widget", {"widget": "missing"}),
            ("click_widget", {"widget": "considerationEngine"}),
            ("set_widget_value", {"widget": "considerationSeconds", "value": 0}),
            ("set_widget_value", {"widget": "considerationMultiPV", "value": 10}),
            ("set_widget_value", {"widget": "considerationEngine", "value": "missing"}),
            ("set_widget_value", {"widget": "considerationArrows", "value": "true"}),
            ("click_table_cell", {"widget": "considerationView", "row": 0, "column": 4}),
        ):
            _, _, error = result_data(await session.call_tool(tool, args))
            assert error, (tool, args)
        await ui.click("considerationEngineSettings")
        dialogs = (await ui.call("list_dialogs"))["windows"]
        settings = next(w for w in dialogs if w["class"] == "ChangeEngineSettingsDialog")
        _, _, error = result_data(await session.call_tool("click_widget", {"widget": "considerationStartStop"}))
        assert error  # main window is blocked by the modal engine settings dialog
        await ui.call("close_dialog", dialog=settings["title"])
        assert (await ui.call("get_app_state"))["ui_state"] == "idle"


async def test_white_drop_arrows(consideration_env):
    async with mcp_session(consideration_env) as session:
        ui = UI(session)
        await ui.call("set_position", sfen="4k4/9/9/9/9/9/9/9/4K4 w g 1")
        await ui.start(multipv=10)
        try:
            arrows = (await ui.board())["arrows"]
            drops = [a for a in arrows if a["from_file"] == 0]
            assert drops and all(a["drop_piece"] == "g" for a in drops)
        finally:
            await ui.stop()


@pytest.mark.parametrize("missing_executable", [False, True])
async def test_unavailable_engine_recovers(consideration_env, missing_executable):
    config = Path(consideration_env["XDG_CONFIG_HOME"]) / "ShogiBoardQ" / "ShogiBoardQ.ini"
    if missing_executable:
        engine = os.environ["SHOGIBOARDQ_TEST_USI_ENGINE"]
        config.write_text(config.read_text().replace(engine, str(config.parent / "missing-engine")))
    else:
        config.write_text("[Engines]\nsize=0\n")
    async with mcp_session(consideration_env) as session:
        ui = UI(session)
        await ui.call("get_app_state")
        await ui.call("show_dock", widget="ConsiderationDock")
        await ui.click("considerationStartStop")
        deadline = asyncio.get_running_loop().time() + 8
        while True:
            dialogs = (await ui.call("list_dialogs"))["windows"]
            errors = [w for w in dialogs if w["class"] == "QMessageBox"]
            if errors:
                break
            assert asyncio.get_running_loop().time() < deadline, dialogs
            await asyncio.sleep(0.1)
        for dialog in errors:
            await ui.call("close_dialog", dialog=dialog["title"])
        await ui.wait("considerationStartStop", "text", "検討開始")
        assert (await ui.call("get_app_state"))["ui_state"] == "idle"


async def test_cancel_immediately_after_start(consideration_env):
    async with mcp_session(consideration_env) as session:
        ui = UI(session)
        await ui.call("get_app_state")
        await ui.call("show_dock", widget="ConsiderationDock")
        for _ in range(3):
            await ui.click("considerationStartStop")
            await ui.wait("considerationStartStop", "text", "検討中止")
            # Do not wait for any PV: cancellation can arrive while isready is pending.
            await ui.stop()
            assert (await ui.call("get_app_state"))["ui_state"] == "idle"
            assert (await ui.board())["arrows"] == []
