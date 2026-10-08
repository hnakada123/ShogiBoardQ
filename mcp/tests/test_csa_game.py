"""CSA UI integration through stdio MCP and an actual shogi-server.

Set SHOGIBOARDQ_TEST_CSA_SERVER to the upstream shogi-server script.
The server, GUI settings, sockets and records are isolated for every test.
"""
from __future__ import annotations

import asyncio
import json
import os
from pathlib import Path
import socket
import shlex
import subprocess
import time

import pytest

from conftest import FIXTURES, mcp_session, result_data

pytestmark = pytest.mark.anyio


@pytest.fixture
def csa_server(tmp_path):
    script = os.environ.get("SHOGIBOARDQ_TEST_CSA_SERVER")
    if not script:
        pytest.skip("SHOGIBOARDQ_TEST_CSA_SERVER is required")
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    with (tmp_path / "server.log").open("w") as log:
        proc = subprocess.Popen(["ruby", script, "--least-time-per-move", "0", "audit", str(port)],
                                cwd=tmp_path, stdout=log, stderr=log)
        try:
            deadline = time.monotonic() + 8
            while True:
                assert proc.poll() is None, (tmp_path / "server.log").read_text()
                try:
                    with socket.create_connection(("127.0.0.1", port), timeout=0.1):
                        break
                except OSError:
                    assert time.monotonic() < deadline
                    time.sleep(0.05)
            yield port, proc
        finally:
            if proc.poll() is None:
                proc.terminate()
            try:
                proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                proc.kill()
                proc.wait()


@pytest.fixture
def csa_env(server_env, app_path, tmp_path):
    env = dict(server_env)
    config = tmp_path / "config" / "ShogiBoardQ"
    config.mkdir(parents=True)
    engine = os.environ.get("SHOGIBOARDQ_TEST_USI_ENGINE", "")
    (config / "ShogiBoardQ.ini").write_text(
        "[General]\nmainWindowSize=@Size(1400 1000)\n"
        f"[Engines]\nsize={int(bool(engine))}\n1\\name=TestUsi\n1\\path={engine}\n",
        encoding="utf-8")
    launcher = tmp_path / "launch-app"
    launcher.write_text(f'#!/bin/sh\nexec {shlex.quote(str(app_path))} "$@" >{shlex.quote(str(tmp_path / "app.log"))} 2>&1\n')
    launcher.chmod(0o700)
    env.update(SHOGIBOARDQ_EXECUTABLE=str(launcher), SHOGIBOARDQ_QUIT_APP_ON_EXIT="1",
               XDG_CONFIG_HOME=str(config.parent), SHOGIBOARDQ_CONFIG_HOME=str(config.parent),
               SHOGIBOARDQ_AUTOMATION_SOCKET=str(tmp_path / "app.sock"))
    return env


class CsaUI:
    def __init__(self, session):
        self.session = session

    async def call(self, tool, **args):
        text, data, error = result_data(await self.session.call_tool(tool, args))
        assert not error, f"{tool}: {text}"
        return data

    async def dialog(self, cls, timeout=8):
        deadline = asyncio.get_running_loop().time() + timeout
        while True:
            windows = (await self.call("list_dialogs"))["windows"]
            found = next((w for w in windows if w["class"] == cls), None)
            if found:
                return found["title"]
            assert asyncio.get_running_loop().time() < deadline, windows
            await asyncio.sleep(0.05)

    async def wait_state(self, **expected):
        deadline = asyncio.get_running_loop().time() + 8
        while True:
            state = await self.call("get_app_state")
            if all(state[k] == v for k, v in expected.items()):
                return state
            assert asyncio.get_running_loop().time() < deadline, state
            await asyncio.sleep(0.05)

    async def connect(self, port, side="b", game="audit-300-5", engine=False):
        await self.call("trigger_action", name="actionCSA")
        title = await self.dialog("CsaGameDialog")
        for widget, value in [("radioButtonEngine" if engine else "radioButtonHuman", True),
                              ("lineEditHost", "127.0.0.1"), ("spinBoxPort", port),
                              ("lineEditId", "BoardQ"), ("checkBoxShowPassword", True),
                              ("lineEditPassword", f"{game}-{side},audit-secret")]:
            await self.call("set_widget_value", target=title, widget=widget, value=value)
        await self.call("set_widget_value", target=title, widget="checkBoxShowPassword", value=False)
        await self.call("click_dialog_button", dialog=title, widget="pushButtonStart")
        return await self.dialog("CsaWaitingDialog")

    async def move(self, usi, decline_promotion=False):
        if "*" in usi:
            await self.call("click_board_square", file=10, rank="PLNSGBR".index(usi[0]) + 1)
            await self.call("click_board_square", file=int(usi[2]), rank=ord(usi[3])-ord("a")+1)
            return
        for file, rank in ((usi[0], usi[1]), (usi[2], usi[3])):
            await self.call("click_board_square", file=int(file), rank=ord(rank)-ord("a")+1)
        if usi.endswith("+") or decline_promotion:
            title = await self.dialog("PromoteDialog")
            await self.call("click_dialog_button", dialog=title, text="成る" if usi.endswith("+") else "成らない")

    async def dismiss_end(self):
        title = await self.dialog("QMessageBox")
        data = await self.call("get_widget_text", dialog=title)
        await self.call("close_dialog", dialog=title)
        await self.wait_state(ui_state="idle", play_mode="not_started")
        return str(data)


class Peer:
    def __init__(self, reader, writer):
        self.reader, self.writer = reader, writer
        self.lines = []

    @classmethod
    async def connect(cls, port, side="w", game="audit-300-5"):
        peer = cls(*(await asyncio.open_connection("127.0.0.1", port)))
        await peer.send(f"LOGIN Peer {game}-{side}")
        return peer

    async def send(self, line):
        self.writer.write((line + "\n").encode())
        await self.writer.drain()

    async def until(self, prefix, timeout=8):
        async with asyncio.timeout(timeout):
            while True:
                line = (await self.reader.readline()).decode().strip()
                assert line, self.lines
                self.lines.append(line)
                if line.startswith(prefix):
                    return line

    async def start(self):
        await self.until("END Game_Summary")
        await self.send("AGREE")
        await self.until("START:", timeout=30)

    async def close(self):
        self.writer.close()
        await self.writer.wait_closed()


@pytest.mark.parametrize("side", ["b", "w"])
async def test_human_moves_resign_and_rematch(csa_env, csa_server, side, tmp_path):
    port, _ = csa_server
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        for _ in range(2):
            await ui.connect(port, side)
            peer = await Peer.connect(port, "w" if side == "b" else "b")
            try:
                await peer.start()
                await ui.wait_state(ui_state="csa_game")
                if side == "b":
                    await ui.move("7g7f")
                    await peer.until("+7776FU")
                    await peer.send("-3334FU")
                else:
                    await peer.send("+7776FU")
                    await ui.wait_state(total_plies=1)
                    await ui.move("3c3d")
                await peer.until("-3334FU")
                await ui.wait_state(total_plies=2)
                position = await ui.call("get_position")
                assert position["moves"] == ["7g7f", "3c3d"], position
                await ui.call("capture_screenshot", output_dir=str(tmp_path))
                if side == "w":
                    await peer.send("+2726FU")
                    await peer.until("+2726FU")
                    await ui.wait_state(total_plies=3)
                await ui.call("trigger_action", name="actionResign")
                await peer.until("#WIN")
                assert "投了" in await ui.dismiss_end()
                await ui.call("save_kifu", path=str(tmp_path / "game.kif"), overwrite=True)
                saved = (tmp_path / "game.kif").read_text(encoding="cp932")
                assert "BoardQ" in saved and "Peer" in saved, saved
                # The time control comes from Game_Summary (audit-300-5), and the end time is recorded.
                assert "持ち時間：05:00+5" in saved and "終了日時：" in saved, saved
                converted = await ui.call("convert_kifu", input_path=str(tmp_path / "game.kif"), output_format="usi")
                assert "7g7f 3c3d" in converted["text"], converted
            finally:
                await peer.close()


async def test_wait_cancel_and_retry(csa_env, csa_server):
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.call("load_kifu", path=str(FIXTURES / "test_basic.kif"))
        before = await ui.call("get_position")
        for _ in range(2):
            title = await ui.connect(csa_server[0])
            await ui.call("click_dialog_button", dialog=title, text="対局キャンセル")
            await ui.wait_state(ui_state="idle", play_mode="not_started")
            assert await ui.call("get_position") == before


async def test_opponent_resign(csa_env, csa_server):
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.connect(csa_server[0])
        peer = await Peer.connect(csa_server[0])
        try:
            await peer.start()
            await ui.wait_state(ui_state="csa_game")
            await ui.move("7g7f")
            await peer.until("+7776FU")
            await peer.send("%TORYO")
            await peer.until("#LOSE")
            assert "投了" in await ui.dismiss_end()
        finally:
            await peer.close()


async def test_waiting_log_and_mcp_enter(csa_env, csa_server):
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        title = await ui.connect(csa_server[0])
        await asyncio.sleep(0.2)
        await ui.call("click_dialog_button", dialog=title, text="通信ログ")
        log = await ui.call("get_widget_text", dialog="csaWaitingLogWindow", widget="csaWaitingLogView")
        text = log["widgets"][0]["text"]
        assert "LOGIN:BoardQ OK" in text
        assert "audit-secret" not in text
        await ui.call("set_widget_value", target="csaWaitingLogWindow", widget="csaWaitingCommandInput",
                      value="LOGOUT", submit=True)
        await ui.wait_state(ui_state="idle", play_mode="not_started")


@pytest.mark.parametrize("language,font_size", [("ja_JP", 10), ("ja_JP", 18), ("en", 18)])
async def test_connection_form_feedback(csa_env, tmp_path, language, font_size):
    config = Path(csa_env["XDG_CONFIG_HOME"]) / "ShogiBoardQ" / "ShogiBoardQ.ini"
    config.write_text(config.read_text() + f"\n[%General]\nlanguage={language}\n"
                      + f"\n[FontSize]\ncsaGameDialog={font_size}\n")
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.call("trigger_action", name="actionCSA")
        title = await ui.dialog("CsaGameDialog")

        async def widget(name):
            return (await ui.call("get_widget_text", dialog=title, widget=name))["widgets"][0]

        assert not (await widget("pushButtonStart"))["enabled"]
        assert not (await widget("comboBoxEngine"))["enabled"]
        assert not (await widget("pushButtonEngineSettings"))["enabled"]
        assert (await widget("labelValidation"))["text"]
        await ui.call("capture_screenshot", target=title, output_dir=str(tmp_path))
        for name, value in [("lineEditHost", "127.0.0.1"), ("lineEditId", "BoardQ"),
                            ("checkBoxShowPassword", True), ("lineEditPassword", "test-600-10,pw")]:
            await ui.call("set_widget_value", target=title, widget=name, value=value)
        assert (await widget("pushButtonStart"))["enabled"]
        await ui.call("set_widget_value", target=title, widget="lineEditId", value="bad id")
        assert not (await widget("pushButtonStart"))["enabled"]
        assert ("空白" if language == "ja_JP" else "whitespace") in (await widget("labelValidation"))["text"]
        await ui.call("close_dialog", dialog=title)


async def test_log_send_buttons_and_disconnect(csa_env, csa_server, tmp_path):
    for name in ("disconnected", "waiting", "connected"):
        (tmp_path / name).mkdir()
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.call("show_dock", widget="CsaLogDock")
        before = await ui.call("get_widget_text", widget="csaCommandInput")
        assert not before["widgets"][0]["enabled"]
        await ui.call("capture_screenshot", output_dir=str(tmp_path / "disconnected"))
        title = await ui.connect(csa_server[0])
        await ui.call("click_dialog_button", dialog=title, text="通信ログ")
        await ui.call("set_widget_value", target="csaWaitingLogWindow", widget="csaWaitingCommandInput",
                      value="%%WHO")
        await ui.call("click_widget", target="csaWaitingLogWindow", widget="csaWaitingSendButton")
        deadline = asyncio.get_running_loop().time() + 5
        while True:
            data = await ui.call("get_widget_text", dialog="csaWaitingLogWindow", widget="csaWaitingLogView")
            if "##[WHO]" in data["widgets"][0]["text"]:
                break
            assert asyncio.get_running_loop().time() < deadline
            await asyncio.sleep(0.05)
        await ui.call("capture_screenshot", target="csaWaitingLogWindow", output_dir=str(tmp_path / "waiting"))
        await ui.call("close_dialog", dialog="csaWaitingLogWindow")
        peer = await Peer.connect(csa_server[0])
        try:
            await peer.start()
            await ui.wait_state(ui_state="csa_game")
            await ui.call("show_dock", widget="CsaLogDock")
            await ui.call("set_widget_value", widget="csaCommandInput", value="%%WHO")
            await ui.call("click_widget", widget="csaSendButton")
            assert (await ui.call("get_widget_text", widget="csaCommandInput"))["widgets"][0]["text"] == ""
            deadline = asyncio.get_running_loop().time() + 5
            while True:
                log = (await ui.call("get_widget_text", widget="csaLogView"))["widgets"][0]["text"]
                if log.count("##[WHO] +OK") >= 2:
                    break
                assert asyncio.get_running_loop().time() < deadline
                await asyncio.sleep(0.05)
            assert (await ui.call("get_app_state"))["ui_state"] == "csa_game"
            await ui.move("7g7f")
            await peer.until("+7776FU")
            await ui.wait_state(current_ply=1)
            await ui.call("capture_screenshot", output_dir=str(tmp_path / "connected"))
            # Ending the TCP session must immediately disable both ways of sending.
            await ui.call("set_widget_value", widget="csaCommandInput", value="LOGOUT", submit=True)
            await ui.dismiss_end()
            assert not (await ui.call("get_widget_text", widget="csaCommandInput"))["widgets"][0]["enabled"]
            assert not (await ui.call("get_widget_text", widget="csaSendButton"))["widgets"][0]["enabled"]
        finally:
            await peer.close()


@pytest.mark.parametrize("action,command", [("actionBreakOffGame", "%CHUDAN"),
                                            ("actionNyugyokuDeclaration", "%KACHI")])
async def test_server_adjudicates_declarations(csa_env, csa_server, action, command):
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.connect(csa_server[0])
        peer = await Peer.connect(csa_server[0])
        try:
            await peer.start()
            await ui.wait_state(ui_state="csa_game")
            await ui.call("trigger_action", name=action)
            if command == "%KACHI":
                # The declaration is confirmed first, as in local games.
                title = await ui.dialog("QMessageBox")
                await ui.call("click_dialog_button", dialog=title, text="宣言する")
            # shogi-server rejects CHUDAN and a declaration at the initial position.
            await peer.until("#WIN")
            assert "反則" in await ui.dismiss_end()
            log = await ui.call("get_widget_text", widget="csaLogView")
            assert command in log["widgets"][0]["text"]
        finally:
            await peer.close()


async def test_disconnect_recovers_ui(csa_env, csa_server):
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.connect(csa_server[0])
        peer = await Peer.connect(csa_server[0])
        try:
            await peer.start()
            await ui.wait_state(ui_state="csa_game")
            csa_server[1].kill()
            assert "中断" in await ui.dismiss_end()
            actions = await ui.call("list_actions")
            assert next(a for a in actions["actions"] if a["name"] == "actionCSA")["enabled"]
        finally:
            await peer.close()


async def test_server_time_up(csa_env, csa_server):
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.connect(csa_server[0], game="audit-0-1")
        peer = await Peer.connect(csa_server[0], game="audit-0-1")
        try:
            await peer.start()
            await asyncio.sleep(1.3)
            await ui.move("7g7f")
            await peer.until("#WIN", timeout=15)
            assert "時間切れ" in await ui.dismiss_end()
        finally:
            await peer.close()


async def test_fischer_time(csa_env, csa_server, tmp_path):
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.connect(csa_server[0], game="audit-300-5F")
        peer = await Peer.connect(csa_server[0], game="audit-300-5F")
        try:
            await peer.start()
            state = await ui.wait_state(ui_state="csa_game")
            assert state["csa"]["black_remaining_ms"] == 305000
            await ui.move("7g7f")
            await peer.until("+7776FU")
            await peer.send("-3334FU")
            await peer.until("-3334FU")
            state = await ui.wait_state(current_ply=2)
            csa = state["csa"]
            assert csa["black_remaining_ms"] + csa["black_consumed_ms"] == 310000
            assert csa["white_remaining_ms"] + csa["white_consumed_ms"] == 305000
            await ui.call("trigger_action", name="actionResign")
            await peer.until("#WIN")
            await ui.dismiss_end()
            # The increment is recorded apart from byoyomi and exported as the third $TIME field.
            await ui.call("save_kifu", path=str(tmp_path / "fischer.kif"), overwrite=True)
            assert "持ち時間：05:00+5秒加算" in (tmp_path / "fischer.kif").read_text(encoding="cp932")
            converted = await ui.call("convert_kifu", input_path=str(tmp_path / "fischer.kif"), output_format="csa")
            assert "$TIME:300+0+5" in converted["text"], converted
        finally:
            await peer.close()


@pytest.mark.parametrize("side", ["b", "w"])
async def test_real_usi_engine(csa_env, csa_server, side):
    if not os.environ.get("SHOGIBOARDQ_TEST_USI_ENGINE"):
        pytest.skip("SHOGIBOARDQ_TEST_USI_ENGINE is required")
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.connect(csa_server[0], side, game="audit-0-3", engine=True)
        peer = await Peer.connect(csa_server[0], "w" if side == "b" else "b", game="audit-0-3")
        try:
            await peer.start()
            if side == "w":
                await peer.send("+7776FU")
            await peer.until("+" if side == "b" else "-", timeout=20)
            state = await ui.wait_state(current_ply=1 if side == "b" else 2)
            assert state["csa"]["is_human"] is False
            await peer.send("%TORYO")
            await peer.until("#LOSE")
            assert "勝ち" in await ui.dismiss_end()
        finally:
            await peer.close()


async def test_engine_initialization_before_agree(csa_env, csa_server, tmp_path):
    # Deterministically deliver Game_Summary while USI is waiting for readyok.
    engine = tmp_path / "slow-engine"
    engine.write_text("""#!/usr/bin/env python3
import sys, time
for command in sys.stdin:
    command = command.strip()
    if command == 'usi':
        print('id name SlowReady\\nusiok', flush=True)
    elif command == 'isready':
        time.sleep(1)
        print('readyok', flush=True)
    elif command.startswith('go '):
        print('bestmove 7g7f', flush=True)
    elif command == 'quit':
        break
""")
    engine.chmod(0o700)
    config = Path(csa_env["XDG_CONFIG_HOME"]) / "ShogiBoardQ" / "ShogiBoardQ.ini"
    config.write_text(f"[Engines]\nsize=1\n1\\name=SlowReady\n1\\path={engine}\n")
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.connect(csa_server[0], engine=True)
        peer = await Peer.connect(csa_server[0])
        try:
            await peer.start()
            await peer.until("+7776FU")
            await ui.wait_state(current_ply=1)
            await peer.send("%TORYO")
            await peer.until("#LOSE")
            await ui.dismiss_end()
        finally:
            await peer.close()


def write_fake_engine(tmp_path, csa_env, mode):
    """Register a USI engine that answers `go` according to mode (win / stop / invalid)."""
    engine = tmp_path / f"fake-{mode}"
    engine.write_text(f"""#!/usr/bin/env python3
import sys
for command in sys.stdin:
    command = command.strip()
    if command == 'usi':
        print('id name Fake\\nusiok', flush=True)
    elif command == 'isready':
        print('readyok', flush=True)
    elif command.startswith('go') and {mode!r} == 'win':
        print('bestmove win', flush=True)
    elif command.startswith('go') and {mode!r} == 'invalid':
        print('bestmove 9z9z', flush=True)
    elif command == 'stop' and {mode!r} == 'stop':
        print('bestmove 7g7f', flush=True)
    elif command == 'quit':
        break
""")
    engine.chmod(0o700)
    config = Path(csa_env["XDG_CONFIG_HOME"]) / "ShogiBoardQ" / "ShogiBoardQ.ini"
    config.write_text(f"[Engines]\nsize=1\n1\\name=Fake\n1\\path={engine}\n")


async def test_engine_declares_entering_king(csa_env, csa_server, tmp_path):
    # bestmove win is sent as %KACHI and the server judges it (illegal at the initial position).
    write_fake_engine(tmp_path, csa_env, "win")
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.connect(csa_server[0], engine=True)
        peer = await Peer.connect(csa_server[0])
        try:
            await peer.start()
            await peer.until("%KACHI")
            await peer.until("#WIN")
            text = await ui.dismiss_end()
            assert "負け" in text and "反則" in text, text
        finally:
            await peer.close()


async def test_engine_moves_immediately_on_request(csa_env, csa_server, tmp_path):
    write_fake_engine(tmp_path, csa_env, "stop")
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.connect(csa_server[0], engine=True)
        peer = await Peer.connect(csa_server[0])
        try:
            await peer.start()
            await ui.wait_state(ui_state="csa_game")
            await asyncio.sleep(0.5)
            await ui.call("trigger_action", name="actionMakeImmediateMove")
            await peer.until("+7776FU")
            await peer.send("%TORYO")
            await peer.until("#LOSE")
            await ui.dismiss_end()
        finally:
            await peer.close()


async def test_engine_error_ends_game(csa_env, csa_server, tmp_path):
    # An unusable engine reply must not leave the window in the CSA game state.
    write_fake_engine(tmp_path, csa_env, "invalid")
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.connect(csa_server[0], engine=True)
        peer = await Peer.connect(csa_server[0])
        try:
            await peer.start()
            text = await ui.dismiss_end()
            assert "中断" in text, text
            error = await ui.dialog("QMessageBox")
            await ui.call("close_dialog", dialog=error)
            await peer.until("#WIN")
            state = await ui.wait_state(ui_state="idle", play_mode="not_started")
            assert state.get("csa", {}).get("state") != "InGame", state
        finally:
            await peer.close()


@pytest.mark.parametrize("side", ["b", "w"])
async def test_capture_promotion_drop_and_navigation(csa_env, csa_server, side, tmp_path):
    sequence = [("7g7f", "+7776FU"), ("3c3d", "-3334FU"),
                ("8h2b+", "+8822UM"), ("8c8d", "-8384FU"),
                ("2b3a", "+2231UM"), ("8d8e", "-8485FU"),
                ("B*5e", "+0055KA"), ("9c9d", "-9394FU")]
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.connect(csa_server[0], side)
        peer = await Peer.connect(csa_server[0], "w" if side == "b" else "b")
        try:
            await peer.start()
            await ui.wait_state(ui_state="csa_game")
            expected = []
            for ply, (usi, csa) in enumerate(sequence, start=1):
                if (csa[0] == "+") == (side == "b"):
                    await ui.move(usi)
                else:
                    await peer.send(csa)
                await peer.until(csa)
                await ui.wait_state(current_ply=ply)
                expected.append(usi)
                position = await ui.call("get_position")
                assert position["moves"] == expected, position
                assert position["sfen"].endswith(f" {ply + 1}")
            if side == "b":
                await ui.call("trigger_action", name="actionResign")
                await peer.until("#WIN")
            else:
                await peer.send("%TORYO")
                await peer.until("#LOSE")
            await ui.dismiss_end()
            for format in ("kif", "csa", "jkf", "usi"):
                path = tmp_path / f"promotion.{format}"
                await ui.call("save_kifu", path=str(path), overwrite=True)
                converted = await ui.call("convert_kifu", input_path=str(path), output_format="usi")
                assert " ".join(expected) in converted["text"], converted
            await ui.call("goto_ply", ply=0)
            assert (await ui.call("get_position"))["moves"] == []
            await ui.call("goto_ply", ply=8)
            assert (await ui.call("get_position"))["moves"] == expected
            await ui.call("capture_screenshot", output_dir=str(tmp_path))
        finally:
            await peer.close()


# 双方の玉が往復するだけの千日手。shogi-server は開始局面を出現回数に数えないので、
# 1手目の後の局面を繰り返し、13手目で4回目にする
SENNICHITE_MOVES = [("7g7f", "+7776FU")] + [("5a5b", "-5152OU"), ("5i5h", "+5958OU"),
                                            ("5b5a", "-5251OU"), ("5h5i", "+5859OU")] * 3

# 後手の角が 3七・4六 を往復して毎手王手をかけ、先手玉が 5九・6八 を往復する（連続王手の千日手）
_OUTE_CYCLE = [("5i6h", "+5968OU"), ("3g4f", "-3746KA"),
               ("6h5i", "+6859OU"), ("4f3g", "-4637KA")]
OUTE_SENNICHITE = {
    # △3七角の王手（6・10・14・18手目）で同じ局面が4回目。手番は先手（勝者）
    "check": [("5g5f", "+5756FU"), ("3c3d", "-3334FU"), ("3g3f", "+3736FU"), ("2b5e", "-2255KA"),
              ("9g9f", "+9796FU"), ("5e3g", "-5537KA")] + _OUTE_CYCLE * 3,
    # ▲6八玉で逃げた局面（7・11・15・19手目）が4回目。手番は王手を続けた後手
    "escape": [("5g5f", "+5756FU"), ("3c3d", "-3334FU"), ("3g3f", "+3736FU"), ("2b5e", "-2255KA"),
               ("5i6h", "+5968OU"), ("5e3g", "-5537KA"), ("9g9f", "+9796FU")]
              + (_OUTE_CYCLE[1:] + _OUTE_CYCLE[:1]) * 3,
}
# 終局行・KIF の結び・CSA/JKF の終局（どちらも後手の反則で先手の勝ち）
OUTE_SENNICHITE_RESULT = {
    "check": ("▲反則勝ち", "まで18手で先手の勝ち", "%-ILLEGAL_ACTION", "-ILLEGAL_ACTION"),
    "escape": ("△反則負け", "まで19手で先手の勝ち", "%ILLEGAL_MOVE", "ILLEGAL_MOVE"),
}


async def play_csa_moves(ui, side, sequence, play_peer, wait_peer):
    """Play a CSA game where the UI plays `side` and the peer plays the other side.

    wait_peer(ply, csa) returns after the peer has received the ply-th move."""
    for ply, (usi, csa) in enumerate(sequence, start=1):
        if (csa[0] == "+") == (side == "b"):
            # 後手の角が先手の陣地（7〜9段目）に出入りする手は成らずに指す
            await ui.move(usi, decline_promotion=csa.endswith("KA") and any(r in "ghi" for r in (usi[1], usi[3])))
        else:
            await play_peer(csa)
        await wait_peer(ply, csa)
        if ply < len(sequence):
            await ui.wait_state(current_ply=ply)


async def assert_saved_results(ui, tmp_path, name, terminal, kif_result, csa_result, jkf_result):
    """Saved records keep the result, and loading them back gives the same terminal line."""
    paths = {fmt: tmp_path / f"{name}.{fmt}" for fmt in ("kif", "csa", "jkf")}
    for path in paths.values():
        await ui.call("save_kifu", path=str(path), overwrite=True)
    assert kif_result in paths["kif"].read_bytes().decode("cp932")
    assert csa_result in paths["csa"].read_text(encoding="utf-8").splitlines()
    assert json.loads(paths["jkf"].read_text(encoding="utf-8"))["moves"][-1]["special"] == jkf_result
    for path in paths.values():
        await ui.call("load_kifu", path=str(path), discard_unsaved=True)
        assert (await ui.call("get_kifu"))["moves"][-1]["text"] == terminal, path


async def test_sennichite_draw(csa_env, csa_server, tmp_path):
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.connect(csa_server[0], "b")
        peer = await Peer.connect(csa_server[0], "w")
        try:
            await peer.start()
            await ui.wait_state(ui_state="csa_game")
            await play_csa_moves(ui, "b", SENNICHITE_MOVES, peer.send, lambda _, csa: peer.until(csa))
            await peer.until("#SENNICHITE")
            assert (await peer.until("#")) == "#DRAW"
            assert "千日手" in await ui.dismiss_end()
            moves = (await ui.call("get_kifu"))["moves"]
            assert len(moves) == 14 and "千日手" in moves[-1]["text"], moves
            await ui.call("save_kifu", path=str(tmp_path / "sennichite.kif"), overwrite=True)
            assert "まで13手で千日手" in (tmp_path / "sennichite.kif").read_bytes().decode("cp932")
            await ui.call("save_kifu", path=str(tmp_path / "sennichite.csa"), overwrite=True)
            assert "%SENNICHITE" in (tmp_path / "sennichite.csa").read_text(encoding="utf-8").splitlines()
        finally:
            await peer.close()


@pytest.mark.parametrize(("completed_by", "side"), [("check", "b"), ("escape", "b"), ("check", "w")])
async def test_oute_sennichite_recorded_as_foul(csa_env, csa_server, tmp_path, completed_by, side):
    """Perpetual check is recorded as a foul by the checking side, from the side to move."""
    terminal, kif_result, csa_result, jkf_result = OUTE_SENNICHITE_RESULT[completed_by]
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.connect(csa_server[0], side)
        peer = await Peer.connect(csa_server[0], "w" if side == "b" else "b")
        try:
            await peer.start()
            await ui.wait_state(ui_state="csa_game")
            await play_csa_moves(ui, side, OUTE_SENNICHITE[completed_by], peer.send,
                                 lambda _, csa: peer.until(csa))
            await peer.until("#OUTE_SENNICHITE")
            # 王手を続けたのは後手（UI が先手なら相手の負け）
            assert (await peer.until("#")) == ("#LOSE" if side == "b" else "#WIN")
            assert ("勝ち" if side == "b" else "負け") in await ui.dismiss_end()
            moves = (await ui.call("get_kifu"))["moves"]
            assert moves[-1]["text"] == terminal, moves[-3:]
            await ui.call("capture_screenshot", output_dir=str(tmp_path))
            await assert_saved_results(ui, tmp_path, f"oute-{completed_by}-{side}", terminal,
                                       kif_result, csa_result, jkf_result)
        finally:
            await peer.close()


async def test_shogihome_oute_sennichite(csa_env, csa_server, tmp_path):
    """ShogiHome checks perpetually as White and receives the same result from shogi-server."""
    endpoint = shogihome_endpoint()
    terminal, kif_result, csa_result, jkf_result = OUTE_SENNICHITE_RESULT["check"]
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        port = csa_server[0]
        game = f"boardq-oute-{os.getpid()}-300-5"
        await ui.connect(port, side="b", game=game)
        await shogihome_login(endpoint, port, f"{game}-w")
        try:
            await ui.wait_state(ui_state="csa_game")

            async def home_move(csa):
                await shogihome_eval(endpoint, f"electronShogiAPI.csaMove(csaAudit.id, {json.dumps(csa)})")

            async def home_saw(ply, csa):
                # 同じ指し手が繰り返し届くので、手数で待つ
                deadline = asyncio.get_running_loop().time() + 8
                while len(moves := (await shogihome_eval(endpoint, "csaAudit"))["moves"]) < ply:
                    assert asyncio.get_running_loop().time() < deadline, (ply, csa)
                    await asyncio.sleep(0.05)
                assert moves[ply - 1].startswith(csa), (ply, moves)

            await play_csa_moves(ui, "b", OUTE_SENNICHITE["check"], home_move, home_saw)
            assert "勝ち" in await ui.dismiss_end()
            peer = await shogihome_eval(endpoint, "csaAudit")
            assert len(peer["moves"]) == 18, peer
            assert peer["results"], peer
            moves = (await ui.call("get_kifu"))["moves"]
            assert moves[-1]["text"] == terminal, moves[-3:]
            (tmp_path / "shogihome-result.json").write_text(json.dumps(peer["results"], ensure_ascii=False))
            await ui.call("capture_screenshot", output_dir=str(tmp_path))
            await assert_saved_results(ui, tmp_path, "shogihome-oute", terminal, kif_result, csa_result, jkf_result)
        finally:
            await shogihome_eval(endpoint, "electronShogiAPI.csaLogout(csaAudit.id)")


def shogihome_endpoint():
    endpoint = os.environ.get("SHOGIBOARDQ_TEST_SHOGIHOME_CDP")
    if not endpoint:
        pytest.skip("SHOGIBOARDQ_TEST_SHOGIHOME_CDP is required (isolated ShogiHome)")
    return endpoint


async def shogihome_eval(endpoint, expression):
    proc = await asyncio.create_subprocess_exec(
        "node", str(Path(__file__).with_name("shogihome_cdp.mjs")),
        stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
    stdout, stderr = await proc.communicate(json.dumps({"endpoint": endpoint, "expression": expression}).encode())
    assert proc.returncode == 0, stderr.decode()
    return json.loads(stdout)


async def shogihome_login(endpoint, port, password):
    """Log ShogiHome in and agree automatically; moves and results are collected in csaAudit."""
    settings = {"protocolVersion": "v121", "host": "127.0.0.1", "port": port,
                "id": "ShogiHome", "password": password, "tcpKeepalive": {"initialDelay": 10}}
    await shogihome_eval(endpoint, """(async () => { window.csaAudit = {moves: [], results: []};
        if (!window.csaAuditHandlers) {
        electronShogiAPI.onCSAGameSummary((id,s) => {
            csaAudit.summary = JSON.parse(s);
            electronShogiAPI.csaAgree(id, csaAudit.summary.id);
        });
        electronShogiAPI.onCSAStart((id,s) => { csaAudit.started=true; });
        electronShogiAPI.onCSAMove((id,move,times) => { csaAudit.moves.push(move); });
        electronShogiAPI.onCSAGameResult((id,special,result) => { csaAudit.results.push({special,result}); });
        window.csaAuditHandlers = true;
        }
        csaAudit.id = await electronShogiAPI.csaLogin(""" + json.dumps(json.dumps(settings)) + "); return csaAudit.id; })()")


@pytest.mark.parametrize("side", ["b", "w"])
async def test_shogihome_interoperability(csa_env, csa_server, tmp_path, side):
    endpoint = shogihome_endpoint()

    async def home(expression):
        return await shogihome_eval(endpoint, expression)

    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        # An explicitly supplied port allows checking an already running local server.
        port = int(os.environ.get("SHOGIBOARDQ_TEST_SHOGIHOME_SERVER_PORT", csa_server[0]))
        game = f"boardq-ui-{os.getpid()}-300-5"
        await ui.connect(port, side=side, game=game)
        await shogihome_login(endpoint, port, f"{game}-{'w' if side == 'b' else 'b'}")
        try:
            await ui.wait_state(ui_state="csa_game")
            sequence = [("7g7f", "+7776FU"), ("3c3d", "-3334FU"),
                        ("8h2b+", "+8822UM"), ("8c8d", "-8384FU"),
                        ("2b3a", "+2231UM"), ("8d8e", "-8485FU"),
                        ("B*5e", "+0055KA"), ("9c9d", "-9394FU")]
            for ply, (usi, csa) in enumerate(sequence, start=1):
                if (csa[0] == "+") == (side == "b"):
                    await ui.move(usi)
                else:
                    await home(f"electronShogiAPI.csaMove(csaAudit.id, {json.dumps(csa)})")
                await ui.wait_state(current_ply=ply)
                assert (await ui.call("get_position"))["moves"] == [m[0] for m in sequence[:ply]]
            await ui.call("capture_screenshot", output_dir=str(tmp_path))
            if side == "b":
                await ui.call("trigger_action", name="actionResign")
                assert "負け" in await ui.dismiss_end()
            else:
                await home("electronShogiAPI.csaResign(csaAudit.id)")
                assert "勝ち" in await ui.dismiss_end()
            peer = await home("csaAudit")
            assert peer["started"] is True
            assert len(peer["moves"]) == 8, peer
            assert peer["results"], peer
            kifu = (await ui.call("get_kifu", format="kif"))["text"]
            assert "ShogiHome" in kifu, kifu
            await ui.call("save_kifu", path=str(tmp_path / "shogihome.kif"), overwrite=True)
        finally:
            await home("electronShogiAPI.csaLogout(csaAudit.id)")


@pytest.mark.parametrize("replay", [False, True])
async def test_single_chudan_and_individual_time_defaults(csa_env, replay):
    """Supplement shogi-server with its unsupported single-line #CHUDAN case."""
    accepted = asyncio.get_running_loop().create_future()

    def connected(reader, writer):
        accepted.set_result(Peer(reader, writer))

    server = await asyncio.start_server(connected, "127.0.0.1", 0)
    async with server, mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.connect(server.sockets[0].getsockname()[1])
        peer = await accepted
        try:
            await peer.until("LOGIN ")
            moves = "+7776FU,T1\n-3334FU,T1\n" if replay else ""
            await peer.send("LOGIN:BoardQ OK\nBEGIN Game_Summary\nProtocol_Version:1.2\n"
                            "Format:Shogi 1.0\nGame_ID:chudan\nName+:BoardQ\nName-:Peer\n"
                            "Your_Turn:+\nTo_Move:+\nBEGIN Time\nTotal_Time:300\nByoyomi:5\nEND Time\n"
                            "BEGIN Time+\nTotal_Time:100\nEND Time+\n"
                            "BEGIN Position\nPI\n+\n" + moves + "END Position\nEND Game_Summary")
            await peer.until("AGREE")
            await peer.send("START:chudan")
            state = await ui.wait_state(ui_state="csa_game")
            assert state["csa"]["black_remaining_ms"] == 100000
            assert state["csa"]["white_remaining_ms"] == 300000
            assert (await ui.call("get_position"))["moves"] == (["7g7f", "3c3d"] if replay else [])
            await peer.send("#CHUDAN")
            assert "中断" in await ui.dismiss_end()
            assert "中断" in str(await ui.call("get_kifu"))
        finally:
            await peer.close()


async def test_connection_refused_recovers(csa_env):
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        port = sock.getsockname()[1]
    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        await ui.connect(port)
        title = await ui.dialog("QMessageBox")
        await ui.call("close_dialog", dialog=title)
        await ui.wait_state(ui_state="idle", play_mode="not_started")
