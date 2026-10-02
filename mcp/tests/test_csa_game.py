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
               XDG_CONFIG_HOME=str(config.parent), SHOGIBOARDQ_AUTOMATION_SOCKET=str(tmp_path / "app.sock"))
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

    async def move(self, usi):
        if "*" in usi:
            await self.call("click_board_square", file=10, rank="PLNSGBR".index(usi[0]) + 1)
            await self.call("click_board_square", file=int(usi[2]), rank=ord(usi[3])-ord("a")+1)
            return
        for file, rank in ((usi[0], usi[1]), (usi[2], usi[3])):
            await self.call("click_board_square", file=int(file), rank=ord(rank)-ord("a")+1)
        if usi.endswith("+"):
            title = await self.dialog("PromoteDialog")
            await self.call("click_dialog_button", dialog=title, text="OK")

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


async def test_fischer_time(csa_env, csa_server):
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


@pytest.mark.parametrize("side", ["b", "w"])
async def test_shogihome_interoperability(csa_env, csa_server, tmp_path, side):
    endpoint = os.environ.get("SHOGIBOARDQ_TEST_SHOGIHOME_CDP")
    if not endpoint:
        pytest.skip("SHOGIBOARDQ_TEST_SHOGIHOME_CDP is required (isolated ShogiHome)")

    async def home(expression):
        proc = await asyncio.create_subprocess_exec(
            "node", str(Path(__file__).with_name("shogihome_cdp.mjs")),
            stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE, stderr=asyncio.subprocess.PIPE)
        stdout, stderr = await proc.communicate(json.dumps({"endpoint": endpoint, "expression": expression}).encode())
        assert proc.returncode == 0, stderr.decode()
        return json.loads(stdout)

    async with mcp_session(csa_env) as session:
        ui = CsaUI(session)
        # An explicitly supplied port allows checking an already running local server.
        port = int(os.environ.get("SHOGIBOARDQ_TEST_SHOGIHOME_SERVER_PORT", csa_server[0]))
        game = f"boardq-ui-{os.getpid()}-300-5"
        await ui.connect(port, side=side, game=game)
        settings = {"protocolVersion": "v121", "host": "127.0.0.1", "port": port,
                    "id": "ShogiHome", "password": f"{game}-{'w' if side == 'b' else 'b'}",
                    "tcpKeepalive": {"initialDelay": 10}}
        await home("""(async () => { window.csaAudit = {moves: [], results: []};
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
