"""Exercise the real dock widgets through the MCP stdio server, with isolated settings."""
from __future__ import annotations

import asyncio
import os
from pathlib import Path

import pytest

from conftest import mcp_session, result_data

pytestmark = pytest.mark.anyio


@pytest.fixture
def dock_env(server_env, app_path, tmp_path):
    env = dict(server_env)
    config = tmp_path / "config" / "ShogiBoardQ"
    config.mkdir(parents=True)
    (config / "ShogiBoardQ.ini").write_text(
        "[General]\nmainWindowSize=@Size(1400 1000)\n", encoding="utf-8")
    env.update(SHOGIBOARDQ_EXECUTABLE=str(app_path), SHOGIBOARDQ_QUIT_APP_ON_EXIT="1",
               XDG_CONFIG_HOME=str(config.parent), SHOGIBOARDQ_CONFIG_HOME=str(config.parent),
               SHOGIBOARDQ_AUTOMATION_SOCKET=str(tmp_path / "app.sock"))
    env["QT_QPA_PLATFORM"] = os.environ.get("SHOGIBOARDQ_TEST_QPA_PLATFORM", "offscreen")
    return env


class Docks:
    def __init__(self, session):
        self.session = session

    async def call(self, tool, **args):
        text, data, error = result_data(await self.session.call_tool(tool, args))
        assert not error, f"{tool} {args}: {text}"
        return data

    async def list(self):
        return {d["object_name"]: d for d in (await self.call("list_docks"))["docks"]}

    async def wait(self, name, **expected):
        deadline = asyncio.get_running_loop().time() + 5
        while True:
            dock = (await self.list())[name]
            if all(dock[key] == value for key, value in expected.items()):
                return dock
            assert asyncio.get_running_loop().time() < deadline, (name, expected, dock)
            await asyncio.sleep(0.05)

    async def configure(self, name, operation, **args):
        await self.call("configure_dock", widget=name, operation=operation, **args)

    async def error(self, tool, **args):
        text, _, error = result_data(await self.session.call_tool(tool, args))
        assert error, (tool, args, text)
        return text


async def test_dock_float_move_tab_close_and_reopen(dock_env, tmp_path):
    async with mcp_session(dock_env) as session:
        ui = Docks(session)
        docks = await ui.list()
        assert len(docks) == 12
        assert docks["AnalysisResultsDock"]["hidden"]
        assert docks["ThinkingDock"]["exposed"]
        assert not docks["ConsiderationDock"]["hidden"]
        assert not docks["ConsiderationDock"]["exposed"]
        for i, name in enumerate(docks):
            await ui.configure(name, "float", geometry={"x": 80, "y": 80, "width": 700, "height": 400})
            dock = await ui.wait(name, floating=True, hidden=False, exposed=True)
            assert dock["toggle_checked"]
            if name == "ConsiderationDock":
                shot = await ui.call("capture_screenshot", target=name, output_dir=str(tmp_path))
                assert Path(shot["path"]).stat().st_size > 1000
                # Floating controls still work when their window is selected explicitly.
                await ui.call("set_widget_value", target=name, widget="considerationTimed", value=True)
                await ui.call("set_widget_value", target=name, widget="considerationSeconds", value=17)
                widgets = (await ui.call("get_widget_text", dialog=name, widget="considerationSeconds"))["widgets"]
                assert widgets[0]["value"] == 17
            await ui.configure(name, "hide")
            dock = await ui.wait(name, hidden=True, exposed=False)
            assert not dock["toggle_checked"]
            await ui.call("show_dock", widget=name)
            await ui.wait(name, hidden=False, exposed=True, floating=True)
            area = ("left", "right", "top", "bottom")[i % 4]
            await ui.configure(name, "dock", area=area)
            await ui.wait(name, area=area, floating=False, exposed=True)

        await ui.configure("ThinkingDock", "dock", area="bottom")
        await ui.configure("EvalChartDock", "tabify", relative_to="ThinkingDock")
        dock = await ui.wait("EvalChartDock", exposed=True, floating=False, area="bottom")
        assert "ThinkingDock" in dock["tabified_with"]
        await ui.wait("ThinkingDock", exposed=False, hidden=False)
        await ui.call("show_dock", widget="ThinkingDock")
        await ui.wait("ThinkingDock", exposed=True)
        await ui.wait("EvalChartDock", exposed=False, hidden=False)
        shot = await ui.call("capture_screenshot", output_dir=str(tmp_path))
        assert Path(shot["path"]).stat().st_size > 1000


async def test_reset_includes_analysis_results(dock_env):
    async with mcp_session(dock_env) as session:
        ui = Docks(session)
        for name in await ui.list():
            await ui.configure(name, "float")
        await ui.call("trigger_action", name="actionResetDockLayout")
        await ui.wait("AnalysisResultsDock", floating=False, hidden=True, area="bottom")
        docks = await ui.list()
        assert all(not d["floating"] for d in docks.values())
        assert docks["RecordPaneDock"]["area"] == "right"
        assert docks["ThinkingDock"]["exposed"]
        for name, dock in docks.items():
            assert dock["hidden"] == (name in {"MenuWindowDock", "JosekiWindowDock", "AnalysisResultsDock"})
        # Repeated resets must not accumulate stale tab groups.
        await ui.call("trigger_action", name="actionResetDockLayout")
        await ui.wait("ThinkingDock", exposed=True)
        assert set((await ui.list())["ThinkingDock"]["tabified_with"]) == {
            "EvalChartDock", "GameInfoDock", "UsiLogDock", "CsaLogDock", "CommentDock", "BranchTreeDock", "ConsiderationDock"}


async def test_title_bar_drag_undocks_panel(dock_env):
    async with mcp_session(dock_env) as session:
        ui = Docks(session)
        await ui.call("show_dock", widget="ThinkingDock")
        dock = await ui.wait("ThinkingDock", exposed=True, floating=False)
        rect = dock["geometry"]
        await ui.configure("ThinkingDock", "drag", x=rect["x"] + rect["width"] + 300, y=rect["y"] + 100)
        await ui.wait("ThinkingDock", floating=True, hidden=False)


async def test_locks_persist_and_prevent_movement(dock_env):
    async with mcp_session(dock_env) as session:
        ui = Docks(session)
        await ui.call("trigger_action", name="actionLockDocks")
        await ui.wait("ThinkingDock", movable=False, floatable=False, allowed_areas=[])
        for operation, args in (("float", {}), ("drag", {"x": 2000, "y": 500}), ("dock", {"area": "top"}),
                                ("tabify", {"relative_to": "RecordPaneDock"})):
            message = await ui.error("configure_dock", widget="ThinkingDock", operation=operation, **args)
            assert "locked" in message
        await ui.configure("ThinkingDock", "hide")
        await ui.wait("ThinkingDock", hidden=True)
        await ui.configure("ThinkingDock", "show")
        await ui.wait("ThinkingDock", hidden=False)
    async with mcp_session(dock_env) as session:
        ui = Docks(session)
        actions = (await ui.call("list_actions"))["actions"]
        assert next(a for a in actions if a["name"] == "actionLockDocks")["checked"]
        assert all(not d["movable"] and not d["floatable"] for d in (await ui.list()).values())
        await ui.call("trigger_action", name="actionLockDocks")
        await ui.wait("ThinkingDock", movable=True, floatable=True)
        await ui.configure("ThinkingDock", "float")
        await ui.wait("ThinkingDock", floating=True)


async def test_invalid_and_modally_blocked_operations(dock_env):
    async with mcp_session(dock_env) as session:
        ui = Docks(session)
        before = await ui.list()
        for args in ({"widget": "missing", "operation": "float"},
                     {"widget": "ThinkingDock", "operation": "tabify", "relative_to": "ThinkingDock"},
                     {"widget": "ThinkingDock", "operation": "dock"},
                     {"widget": "ThinkingDock", "operation": "hide", "area": "left"},
                     {"widget": "ThinkingDock", "operation": "tabify", "relative_to": "JosekiWindowDock"},
                     {"widget": "ThinkingDock", "operation": "float", "geometry": {"x": 1}}):
            await ui.error("configure_dock", **args)
        assert {n: d["floating"] for n, d in (await ui.list()).items()} == {n: d["floating"] for n, d in before.items()}
        await ui.call("trigger_action", name="actionSaveDockLayout")
        for _ in range(100):
            windows = (await ui.call("list_dialogs"))["windows"]
            dialogs = [w for w in windows if w["class"] == "QInputDialog"]
            if dialogs:
                break
            await asyncio.sleep(0.05)
        assert dialogs
        await ui.error("configure_dock", widget="ThinkingDock", operation="float")
        await ui.error("trigger_action", name="actionResetDockLayout")
        await ui.call("close_dialog", dialog=dialogs[0]["title"])
