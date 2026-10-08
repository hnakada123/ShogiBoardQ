"""Kifu guide audit through the MCP stdio server and the real Qt application."""
from __future__ import annotations

import asyncio
import json
from pathlib import Path

import pytest

from conftest import FIXTURES, mcp_session, message_box, result_data

pytestmark = pytest.mark.anyio
MOVES = ["7g7f", "3c3d", "2g2f", "8c8d", "2f2e", "8d8e", "6i7h"]


@pytest.fixture
def kifu_env(server_env, app_path, tmp_path):
    env = dict(server_env)
    config = tmp_path / "config" / "ShogiBoardQ"
    config.mkdir(parents=True)
    (config / "ShogiBoardQ.ini").write_text(
        "[General]\nmainWindowSize=@Size(1400 1000)\n", encoding="utf-8")
    env.update(SHOGIBOARDQ_EXECUTABLE=str(app_path), SHOGIBOARDQ_QUIT_APP_ON_EXIT="1",
               XDG_CONFIG_HOME=str(config.parent), SHOGIBOARDQ_CONFIG_HOME=str(config.parent),
               XDG_DATA_HOME=str(tmp_path / "data"), SHOGIBOARDQ_DATA_HOME=str(tmp_path / "data"),
               SHOGIBOARDQ_AUTOMATION_SOCKET=str(tmp_path / "app.sock"))
    return env


class KifuUi:
    def __init__(self, session, directory):
        self.session = session
        self.directory = directory

    async def call(self, tool, **args):
        text, data, error = result_data(await self.session.call_tool(tool, args))
        with (self.directory / "mcp.jsonl").open("a", encoding="utf-8") as log:
            log.write(json.dumps(dict(tool=tool, args=args, data=data, error=text if error else None),
                                 ensure_ascii=False) + "\n")
        assert not error, f"{tool} {args}: {text}"
        return data

    async def wait(self, tool, predicate, **args):
        deadline = asyncio.get_running_loop().time() + 5
        while True:
            data = await self.call(tool, **args)
            if predicate(data):
                return data
            assert asyncio.get_running_loop().time() < deadline, data
            await asyncio.sleep(0.05)

    async def dialog(self, title):
        return await self.wait("list_dialogs", lambda d: any(
            title in w["title"] or title == w["object_name"] for w in d["windows"]))

    async def message_box(self, text):
        return await message_box(self.session, text)

    async def copy(self, action):
        await self.call("trigger_action", name=action)
        # Calls on the same connection are processed after the deferred QAction.
        await asyncio.sleep(0.05)
        return await self.call("get_clipboard")

    async def paste(self, text):
        await self.call("trigger_action", name="actionPasteKifu")
        await self.dialog("kifuPasteDialog")
        await self.call("set_widget_value", target="kifuPasteDialog", widget="kifuPasteText", value=text)
        await self.call("click_dialog_button", dialog="kifuPasteDialog", text="取り込む")


@pytest.mark.parametrize("extension", ["kif", "kifu", "ki2", "ki2u", "csa", "jkf", "usi", "usen"])
async def test_save_reload_formats(kifu_env, tmp_path, extension):
    async with mcp_session(kifu_env) as session:
        ui = KifuUi(session, tmp_path)
        await ui.call("load_kifu", path=str(FIXTURES / "test_comments.kif"))
        assert (await ui.call("get_position"))["ply"] == 0
        await ui.call("goto_ply", ply=7)
        original = await ui.call("get_position")
        assert original["moves"] == MOVES
        assert original["ply"] == 7
        path = tmp_path / f"日本語 棋譜.{extension}"
        await ui.call("save_kifu", path=str(path))
        encoded = path.read_bytes()
        decoded = encoded.decode("cp932" if extension in {"kif", "ki2"} else "utf-8")
        if extension in {"kif", "kifu", "ki2", "ki2u", "csa", "jkf"}:
            assert "テスト先手" in decoded and "テスト後手" in decoded
        await ui.call("load_kifu", path=str(path))
        await ui.call("goto_ply", ply=7)
        assert await ui.call("get_position") == original
        assert not (await ui.call("get_app_state"))["dirty"]
        if extension in {"kif", "kifu", "csa", "jkf"}:
            moves = (await ui.call("get_kifu"))["moves"]
            assert "初手のコメント" in moves[0]["comment"]
            assert "10" in moves[0]["time"]
        # Existing destination is guarded; explicit overwrite uses the same encoding/format.
        _, _, error = result_data(await session.call_tool("save_kifu", {"path": str(path)}))
        assert error
        await ui.call("save_kifu", path=str(path), overwrite=True)
        assert path.read_bytes() == encoded


@pytest.mark.parametrize("format,action", [(f, "actionCopy" + a) for f, a in [
    ("kif", "KIF"), ("ki2", "KI2"), ("csa", "CSA"), ("jkf", "JKF"), ("usi", "USIAll"), ("usen", "USEN")]])
async def test_copy_and_paste_formats(kifu_env, tmp_path, format, action):
    async with mcp_session(kifu_env) as session:
        ui = KifuUi(session, tmp_path)
        await ui.call("load_kifu", path=str(FIXTURES / "test_branch.kif"))
        original = await ui.call("get_position")
        text = (await ui.copy(action))["text"]
        converted = await ui.call("convert_kifu", text=text, output_format="usi")
        assert converted["usi_moves"] == MOVES
        assert converted["has_branches"] == (format in {"kif", "ki2", "jkf", "usen"})
        await ui.call("trigger_action", name="actionPasteKifu")
        await ui.dialog("kifuPasteDialog")
        await ui.call("click_dialog_button", dialog="kifuPasteDialog", text="クリップボードから貼り付け")
        widgets = await ui.call("get_widget_text", dialog="kifuPasteDialog", widget="kifuPasteText")
        assert widgets["widgets"][0]["text"] == text
        await ui.call("click_dialog_button", dialog="kifuPasteDialog", text="取り込む")
        await ui.wait("get_app_state", lambda d: d["dirty"])
        assert (await ui.call("get_app_state"))["kifu_file"] == ""
        assert await ui.call("get_position") == original
        reexport = (await ui.call("get_kifu", format="kif"))["text"]
        assert ("変化：" in reexport) == (format in {"kif", "ki2", "jkf", "usen"})


async def test_position_and_current_usi_copy(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = KifuUi(session, tmp_path)
        await ui.call("load_kifu", path=str(FIXTURES / "test_basic.kif"))
        for ply in (0, 3, 7):
            await ui.call("goto_ply", ply=ply)
            pos = await ui.call("get_position")
            text = (await ui.copy("actionCopyUSICurrent"))["text"]
            converted = await ui.call("convert_kifu", text=text, output_format="usi")
            assert converted["usi_moves"] == MOVES[:ply]
            assert (await ui.copy("actionCopySFEN"))["text"] == pos["sfen"]
            bod = (await ui.copy("actionCopyBOD"))["text"]
            converted = await ui.call("convert_kifu", text=bod, output_format="usi")
            assert converted["initial_sfen"].split()[:3] == pos["sfen"].split()[:3]
        image = await ui.copy("actionCopyBoardToClipboard")
        assert image["has_image"] and image["image_width"] > 100 and image["image_height"] > 100
        await ui.call("capture_screenshot", output_dir=str(tmp_path))


async def test_paste_dialog_controls_and_cancel(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = KifuUi(session, tmp_path)
        await ui.call("load_kifu", path=str(FIXTURES / "test_basic.kif"))
        await ui.call("trigger_action", name="actionPasteKifu")
        await ui.dialog("kifuPasteDialog")
        await ui.call("set_widget_value", target="kifuPasteDialog", widget="kifuPasteText", value="discard me")
        await ui.call("click_dialog_button", dialog="kifuPasteDialog", text="クリア")
        assert (await ui.call("get_widget_text", dialog="kifuPasteDialog", widget="kifuPasteText"))["widgets"][0]["text"] == ""
        assert not (await ui.call("get_widget_text", dialog="kifuPasteDialog", widget="importKifu"))["widgets"][0]["enabled"]
        await ui.dialog("kifuPasteDialog")
        await ui.call("close_dialog", dialog="kifuPasteDialog")
        await ui.paste("startpos moves 7g7f 8c8d")
        await ui.wait("get_app_state", lambda d: d["dirty"])
        pos = await ui.call("get_position")
        await ui.paste("startpos moves 2g2f")
        box = await ui.message_box("未保存の変更")
        await ui.call("click_dialog_button", dialog=box, text="キャンセル")
        assert await ui.call("get_position") == pos
        assert (await ui.call("get_app_state"))["dirty"]
        await ui.dialog("kifuPasteDialog")
        assert (await ui.call("get_widget_text", dialog="kifuPasteDialog", widget="kifuPasteText"))["widgets"][0]["text"] == "startpos moves 2g2f"


async def test_file_dialog_filters_and_save(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = KifuUi(session, tmp_path)
        await ui.call("trigger_action", name="actionOpenKifuFile")
        await ui.dialog("棋譜ファイルを開く")
        await ui.call("set_widget_value", target="棋譜ファイルを開く", widget="fileNameEdit", value=str(FIXTURES / "test_basic.kif"))
        widgets = await ui.call("get_widget_text", dialog="棋譜ファイルを開く")
        assert "*.sfen" in str(widgets) and "*.ki2u" in str(widgets)
        button = next(w["text"] for w in widgets["widgets"] if w["class"] == "QPushButton" and ("開く" in w.get("text", "") or "Open" in w.get("text", "")))
        await ui.call("click_dialog_button", dialog="棋譜ファイルを開く", text=button)
        await ui.wait("get_app_state", lambda d: d["total_plies"] == 7)
        await ui.call("trigger_action", name="actionSaveAs")
        await ui.dialog("名前を付けて保存")
        await ui.call("set_widget_value", target="名前を付けて保存", widget="fileTypeCombo", value="JKF形式 (*.jkf)")
        await ui.call("set_widget_value", target="名前を付けて保存", widget="fileNameEdit", value=str(tmp_path / "拡張子なし"))
        widgets = await ui.call("get_widget_text", dialog="名前を付けて保存")
        button = next(w["text"] for w in widgets["widgets"] if w["class"] == "QPushButton" and ("保存" in w.get("text", "") or "Save" in w.get("text", "")))
        await ui.call("click_dialog_button", dialog="名前を付けて保存", text=button)
        await ui.wait("get_app_state", lambda d: d["kifu_file"].endswith("拡張子なし.jkf"))
        assert json.loads((tmp_path / "拡張子なし.jkf").read_text())["moves"]


async def test_branch_current_usi(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = KifuUi(session, tmp_path)
        await ui.call("load_kifu", path=str(FIXTURES / "test_branch.kif"))
        await ui.call("goto_ply", ply=3)
        rows = (await ui.call("get_widget_text", widget="kifuBranchTable"))["widgets"][0]["rows"]
        row = next(i for i, cells in enumerate(rows[1:]) if "６六歩" in str(cells))
        await ui.call("click_table_cell", widget="kifuBranchTable", row=row, column=0)
        await ui.wait("get_widget_text", lambda d: "６六歩" in str(d["widgets"][0]["rows"][4]), widget="kifuTable")
        await ui.call("goto_ply", ply=4)
        pos = await ui.call("get_position")
        assert pos["moves"] == ["7g7f", "3c3d", "6g6f", "8c8d"]
        assert (await ui.call("get_app_state"))["total_plies"] == 6
        _, _, error = result_data(await session.call_tool("goto_ply", {"ply": 7}))
        assert error
        text = (await ui.copy("actionCopyUSICurrent"))["text"]
        assert "6g6f" in text and "2g2f" not in text
        converted = await ui.call("convert_kifu", text=text, output_format="usi")
        assert converted["sfens"][-1] == pos["sfen"]
        assert (await ui.copy("actionCopySFEN"))["text"] == pos["sfen"]
        assert (await ui.copy("actionCopyUSIAll"))["text"].split("moves ")[1].split() == MOVES
        await ui.call("capture_screenshot", output_dir=str(tmp_path))


async def test_current_usi_excludes_future_resignation(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = KifuUi(session, tmp_path)
        await ui.call("load_kifu", path=str(FIXTURES / "test_resign.usi"))
        for ply in (0, 1, 2):
            await ui.call("goto_ply", ply=ply)
            assert "resign" not in (await ui.copy("actionCopyUSICurrent"))["text"]
        await ui.call("goto_ply", ply=3)
        assert "resign" in (await ui.copy("actionCopyUSICurrent"))["text"]


@pytest.mark.parametrize("extension", ["usi", "sfen"])
async def test_loading_record_without_headers_clears_previous_info(kifu_env, tmp_path, extension):
    async with mcp_session(kifu_env) as session:
        ui = KifuUi(session, tmp_path)
        await ui.call("load_kifu", path=str(FIXTURES / "test_comments.kif"))
        assert "テスト先手" in str(await ui.call("get_widget_text"))
        path = FIXTURES / "test_basic.usi"
        if extension == "sfen":
            path = tmp_path / "position.sfen"
            path.write_text((await ui.call("get_position"))["sfen"], encoding="utf-8")
        await ui.call("load_kifu", path=str(path))
        kif = (await ui.call("get_kifu", format="kif"))["text"]
        assert "テスト先手" not in kif and "テスト後手" not in kif
        assert "テスト先手" not in str(await ui.call("get_widget_text"))


async def test_kifu_pagination_reports_truncation(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = KifuUi(session, tmp_path)
        await ui.call("load_kifu", path=str(FIXTURES / "test_basic.kif"))
        assert (await ui.call("get_kifu", max_moves=3))["truncated"]
        assert not (await ui.call("get_kifu", from_ply=5, max_moves=3))["truncated"]


async def test_sfen_and_bod_paste(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = KifuUi(session, tmp_path)
        await ui.call("load_kifu", path=str(FIXTURES / "test_basic.kif"))
        await ui.call("goto_ply", ply=3)
        pos = await ui.call("get_position")
        bod = (await ui.copy("actionCopyBOD"))["text"]
        for text in (pos["sfen"], bod):
            await ui.call("load_kifu", path=str(FIXTURES / "test_basic.kif"), discard_unsaved=True)
            await ui.paste(text)
            await ui.wait("get_app_state", lambda d: d["dirty"] and d["total_plies"] == 0)
            assert (await ui.call("get_position"))["sfen"].split()[:3] == pos["sfen"].split()[:3]
            assert (await ui.copy("actionCopySFEN"))["text"] == (await ui.call("get_position"))["sfen"]


async def test_invalid_paste_preserves_record_and_editor(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = KifuUi(session, tmp_path)
        await ui.call("load_kifu", path=str(FIXTURES / "test_comments.kif"))
        await ui.call("goto_ply", ply=3)
        original = await ui.call("get_position")
        record = await ui.call("get_kifu", format="kif")
        await ui.paste("{ invalid JSON }")
        dialogs = await ui.wait("list_dialogs", lambda d: any(w["class"] == "QMessageBox" for w in d["windows"]))
        dialog = next(w for w in dialogs["windows"] if w["class"] == "QMessageBox")
        await ui.call("close_dialog", dialog=dialog["selector"])
        assert await ui.call("get_position") == original
        assert await ui.call("get_kifu", format="kif") == record
        await ui.dialog("kifuPasteDialog")
        await ui.call("capture_screenshot", target="kifuPasteDialog", output_dir=str(tmp_path))


async def test_sfen_file_detaches_overwrite_target(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = KifuUi(session, tmp_path)
        await ui.call("load_kifu", path=str(FIXTURES / "test_comments.kif"))
        await ui.call("goto_ply", ply=3)
        sfen = (await ui.call("get_position"))["sfen"]
        path = tmp_path / "局面.sfen"
        path.write_text(sfen, encoding="utf-8")
        await ui.call("load_kifu", text="startpos moves 7g7f")
        assert (await ui.call("get_app_state"))["dirty"]
        await ui.call("load_kifu", path=str(path), discard_unsaved=True)
        state = await ui.call("get_app_state")
        assert state["kifu_file"] == "" and not state["dirty"]
        assert state["total_plies"] == 0 and state["sfen"] == sfen
        assert (await ui.call("get_kifu"))["moves"] == []
        assert "テスト先手" not in (await ui.call("get_kifu", format="kif"))["text"]


async def test_unsaved_save_cancel_then_discard(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = KifuUi(session, tmp_path)
        await ui.call("load_kifu", text="startpos moves 7g7f")
        original = await ui.call("get_position")
        await ui.paste("startpos moves 2g2f")
        box = await ui.message_box("未保存の変更")
        await ui.call("click_dialog_button", dialog=box, text="保存")
        await ui.dialog("名前を付けて保存")
        await ui.call("close_dialog", dialog="名前を付けて保存")
        await ui.dialog("kifuPasteDialog")
        assert await ui.call("get_position") == original
        assert (await ui.call("get_app_state"))["dirty"]
        await ui.call("click_dialog_button", dialog="kifuPasteDialog", text="取り込む")
        box = await ui.message_box("未保存の変更")
        await ui.call("click_dialog_button", dialog=box, text="破棄")
        await ui.wait("get_kifu", lambda d: d["moves"][0].get("usi") == "2g2f")
        assert (await ui.call("get_app_state"))["dirty"]


async def test_paste_font_persistence_and_mcp_validation(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = KifuUi(session, tmp_path)
        await ui.call("load_kifu", path=str(FIXTURES / "test_basic.kif"))
        text = (await ui.copy("actionCopyKIF"))["text"]
        clipped = await ui.call("get_clipboard", max_chars=12)
        assert clipped["truncated"] and clipped["text"] == text[:12]
        await ui.call("trigger_action", name="actionPasteKifu")
        await ui.dialog("kifuPasteDialog")
        original = (await ui.call("get_widget_text", dialog="kifuPasteDialog", widget="kifuPasteText"))["widgets"][0]["font_point_size"]
        await ui.call("click_dialog_button", dialog="kifuPasteDialog", text="A+")
        await ui.call("set_widget_value", target="kifuPasteDialog", widget="kifuPasteText", value="<b>plain text</b>")
        editor = (await ui.call("get_widget_text", dialog="kifuPasteDialog", widget="kifuPasteText"))["widgets"][0]
        assert editor["font_point_size"] == original + 1 and editor["text"] == "<b>plain text</b>"
        for value in (True, 42):
            _, _, error = result_data(await session.call_tool("set_widget_value", dict(target="kifuPasteDialog", widget="kifuPasteText", value=value)))
            assert error
        await ui.call("close_dialog", dialog="kifuPasteDialog")
    async with mcp_session(kifu_env) as session:
        ui = KifuUi(session, tmp_path)
        await ui.call("trigger_action", name="actionPasteKifu")
        await ui.dialog("kifuPasteDialog")
        editor = (await ui.call("get_widget_text", dialog="kifuPasteDialog", widget="kifuPasteText"))["widgets"][0]
        assert editor["font_point_size"] == original + 1


async def test_save_filter_warning_and_explicit_extension(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = KifuUi(session, tmp_path)
        await ui.call("load_kifu", text="startpos moves 7g7f 3c3d")
        for name, warning in (("時間省略.ki2u", True), ("拡張子優先.csa", False)):
            path = tmp_path / name
            await ui.call("trigger_action", name="actionSaveAs")
            await ui.dialog("名前を付けて保存")
            await ui.call("set_widget_value", target="名前を付けて保存", widget="fileTypeCombo", value="JKF形式 (*.jkf)")
            await ui.call("set_widget_value", target="名前を付けて保存", widget="fileNameEdit", value=str(path))
            widgets = await ui.call("get_widget_text", dialog="名前を付けて保存")
            button = next(w["text"] for w in widgets["widgets"] if w["class"] == "QPushButton" and ("保存" in w.get("text", "") or "Save" in w.get("text", "")))
            await ui.call("click_dialog_button", dialog="名前を付けて保存", text=button)
            if warning:
                await ui.call("close_dialog", dialog=await ui.message_box("KI2形式は消費時間"))
                assert not path.exists() and (await ui.call("get_app_state"))["dirty"]
            else:
                await ui.wait("get_app_state", lambda d: d["kifu_file"] == str(path))
                assert "+7776FU" in path.read_text(encoding="utf-8")


async def test_game_end_auto_save(kifu_env, tmp_path):
    config = Path(kifu_env["XDG_CONFIG_HOME"]) / "ShogiBoardQ" / "ShogiBoardQ.ini"
    with config.open("a", encoding="utf-8") as ini:
        ini.write(f"[GameSettings]\nkifuSaveDir={tmp_path}\n")
    async with mcp_session(kifu_env) as session:
        ui = KifuUi(session, tmp_path)
        await ui.call("trigger_action", name="actionStartGame")
        await ui.dialog("StartGameDialog")
        for widget in ("comboBoxPlayer1", "comboBoxPlayer2"):
            await ui.call("set_widget_value", target="StartGameDialog", widget=widget, value=0)
        await ui.call("set_widget_value", target="StartGameDialog", widget="checkBoxAutoSaveKifu", value=True)
        # The path field is read-only; its persisted value is visible in the real dialog.
        field = (await ui.call("get_widget_text", dialog="StartGameDialog", widget="lineEditKifuSaveDir"))["widgets"][0]
        assert field["text"] == str(tmp_path)
        _, _, error = result_data(await session.call_tool("set_widget_value", dict(target="StartGameDialog", widget="lineEditKifuSaveDir", value="/invalid")))
        assert error
        await ui.call("click_dialog_button", dialog="StartGameDialog", text="対局開始")
        await ui.wait("get_app_state", lambda d: d["ui_state"] == "game")
        await ui.call("click_board_square", file=7, rank=7)
        await ui.call("click_board_square", file=7, rank=6)
        await ui.wait("get_app_state", lambda d: d["current_ply"] == 1)
        await ui.call("trigger_action", name="actionResign")
        await ui.call("close_dialog", dialog=await ui.message_box("投了"))
        state = await ui.wait("get_app_state", lambda d: bool(d["kifu_file"]))
        path = Path(state["kifu_file"])
        assert path.parent == tmp_path and not state["dirty"]
        text = path.read_text(encoding="utf-8")
        assert "７六歩" in text and "投了" in text
        await ui.call("load_kifu", path=str(path))
        assert (await ui.call("get_kifu"))["moves"][0]["usi"] == "7g7f"
