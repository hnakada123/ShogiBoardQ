"""USI communication log tab: the log of the running game, and the console that automation must not use.

Set SHOGIBOARDQ_TEST_USI_ENGINE to a USI engine (registered as "TestUsi").
"""
from __future__ import annotations

import asyncio
import json
import os

import pytest

from conftest import mcp_session, result_data

pytestmark = pytest.mark.anyio


async def call(session, tool, **args):
    text, data, error = result_data(await session.call_tool(tool, args))
    assert not error, f"{tool}: {text}"
    return data


async def wait_for(predicate, timeout=10.0):
    deadline = asyncio.get_running_loop().time() + timeout
    while True:
        value = await predicate()
        if value:
            return value
        assert asyncio.get_running_loop().time() < deadline, "timed out"
        await asyncio.sleep(0.1)


async def test_manual_usi_command_reaches_running_engine(server_env, tmp_path):
    if not os.environ.get("SHOGIBOARDQ_TEST_USI_ENGINE"):
        pytest.skip("SHOGIBOARDQ_TEST_USI_ENGINE is required")
    # Quit the app this session launches; otherwise it keeps running after the test suite.
    env = dict(server_env, SHOGIBOARDQ_QUIT_APP_ON_EXIT="1",
               SHOGIBOARDQ_AUTOMATION_SOCKET=str(tmp_path / "app.sock"))
    async with mcp_session(env) as session:
        await call(session, "trigger_action", name="actionStartGame")
        await wait_for(lambda: _has_dialog(session, "StartGameDialog"))
        await call(session, "set_widget_value", target="StartGameDialog", widget="comboBoxPlayer1", value=0)
        await call(session, "set_widget_value", target="StartGameDialog", widget="comboBoxPlayer2", value=1)
        await call(session, "set_widget_value", target="StartGameDialog", widget="byoyomiSec1", value=30)
        await call(session, "click_dialog_button", target="StartGameDialog", text="対局開始")

        async def engine_ready():
            state = await call(session, "get_app_state")
            if state["play_mode"] != "human_vs_engine":
                return False
            log = json.dumps(await call(session, "get_widget_text", widget="usiLogView"), ensure_ascii=False)
            return "readyok" in log

        await call(session, "show_dock", widget="UsiLogDock")
        await wait_for(engine_ready, timeout=20)

        async def log_text():
            data = await call(session, "get_widget_text", widget="usiLogView")
            return data["widgets"][0]["text"]

        # Send to both engines. In a human-vs-engine game the only engine is E1: it receives the
        # command exactly once, and E2 is reported as absent (USI engines ignore unknown commands).
        probe = "shogiboardq-usi-log-probe"
        await call(session, "set_widget_value", widget="usiCommandTarget", value=2)
        # The console sends any command to the engine (which can write files through its options), so automation
        # cannot use it, even in unrestricted mode.
        text, _, error = result_data(await session.call_tool(
            "set_widget_value", {"widget": "usiCommandInput", "value": probe, "submit": True}))
        assert error and "not_allowed" in text, text
        await asyncio.sleep(0.3)
        assert probe not in await log_text()

        await call(session, "trigger_action", name="actionResign")


async def _has_dialog(session, cls):
    windows = (await call(session, "list_dialogs"))["windows"]
    return any(w["class"] == cls for w in windows)
