"""USI communication log tab: manual commands reach the engines of the running game.

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


async def test_manual_usi_command_reaches_running_engine(server_env):
    if not os.environ.get("SHOGIBOARDQ_TEST_USI_ENGINE"):
        pytest.skip("SHOGIBOARDQ_TEST_USI_ENGINE is required")
    async with mcp_session(server_env) as session:
        await call(session, "trigger_action", name="actionStartGame")
        await wait_for(lambda: _has_dialog(session, "StartGameDialog"))
        await call(session, "set_widget_value", target="StartGameDialog", widget="comboBoxPlayer1", value=0)
        await call(session, "set_widget_value", target="StartGameDialog", widget="comboBoxPlayer2", value=1)
        await call(session, "set_widget_value", target="StartGameDialog", widget="byoyomiSec1", value=30)
        await call(session, "click_dialog_button", dialog="StartGameDialog", text="対局開始")

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
        await call(session, "set_widget_value", widget="usiCommandInput", value=probe, submit=True)

        async def answered():
            text = await log_text()
            return text if f"▶ E1: {probe}" in text and "E2: エンジンが起動していません" in text else None

        text = await wait_for(answered)
        await asyncio.sleep(0.3)
        text = await log_text()
        assert text.count(f"▶ E1: {probe}") == 1, text
        assert "E1: エンジンが起動していません" not in text, text

        await call(session, "trigger_action", name="actionResign")


async def _has_dialog(session, cls):
    windows = (await call(session, "list_dialogs"))["windows"]
    return any(w["class"] == cls for w in windows)
