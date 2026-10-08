"""Game information editing through the MCP server and actual Qt widgets."""
from pathlib import Path

import pytest

from conftest import FIXTURES, mcp_session, result_data
from test_kifu_management import KifuUi, kifu_env

pytestmark = pytest.mark.anyio


class GameInfoUi(KifuUi):
    async def open(self):
        await self.call("load_kifu", path=str(FIXTURES / "test_basic.kif"))
        await self.call("show_dock", widget="GameInfoDock")
        await self.wait("get_widget_text", lambda d: "visible" not in d["widgets"][0], widget="gameInfoTable")

    async def rows(self):
        return (await self.call("get_widget_text", widget="gameInfoTable"))["widgets"][0]["rows"][1:]

    async def edit(self, key, value, **args):
        row = next(i for i, cells in enumerate(await self.rows()) if cells[0] == key)
        await self.call("edit_table_cell", widget="gameInfoTable", row=row, column=1, text=value, **args)
        if args.get("commit", True):
            await self.wait("get_widget_text", lambda d: d["widgets"][0]["rows"][row + 1][1] == value, widget="gameInfoTable")
        else:
            await self.wait("get_widget_text", lambda d: any(
                w["class"] in {"QLineEdit", "QExpandingLineEdit"} and w.get("text") == value
                for w in d["widgets"]))

    async def button(self, name):
        await self.call("click_widget", widget="gameInfo" + name)

    async def button_enabled(self, name):
        widget = (await self.call("get_widget_text", widget="gameInfo" + name))["widgets"][0]
        return widget["enabled"]

    async def assert_button_disabled(self, name):
        assert not await self.button_enabled(name)
        text, _, error = result_data(await self.session.call_tool(
            "click_widget", {"widget": "gameInfo" + name}))
        assert error and "invalid_state" in text

    async def dirty(self):
        return await self.button_enabled("Apply")


async def test_apply_updates_names_and_record_dirty(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = GameInfoUi(session, tmp_path)
        await ui.open()
        await ui.edit("先手", "編集後の先手")
        await ui.edit("後手", "編集後の後手")
        assert await ui.dirty()
        await ui.button("Apply")
        assert not await ui.dirty()
        assert (await ui.call("get_widget_text", widget="blackNameLabel"))["widgets"][0]["text"].endswith("編集後の先手")
        assert (await ui.call("get_widget_text", widget="whiteNameLabel"))["widgets"][0]["text"].endswith("編集後の後手")
        await ui.call("capture_screenshot", output_dir=str(tmp_path))
        assert (await ui.call("get_app_state"))["dirty"]
        path = tmp_path / "edited.kifu"
        await ui.call("save_kifu", path=str(path))
        await ui.call("load_kifu", path=str(path))
        assert ["先手", "編集後の先手"] in await ui.rows()


async def test_unapplied_edits_guard_and_save(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = GameInfoUi(session, tmp_path)
        await ui.open()
        await ui.edit("先手", "未確定の編集")
        _, _, error = result_data(await session.call_tool("load_kifu", {"path": str(FIXTURES / "test_basic.usi")}))
        assert error
        path = tmp_path / "pending.kifu"
        await ui.call("save_kifu", path=str(path))
        assert "未確定の編集" in path.read_text(encoding="utf-8")
        assert not await ui.dirty()
        assert not (await ui.call("get_app_state"))["dirty"]


async def test_undo_redo_multiple_cells(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = GameInfoUi(session, tmp_path)
        await ui.open()
        await ui.edit("先手", "先手編集")
        await ui.edit("後手", "後手編集")
        await ui.button("Undo")
        assert ["先手", "先手編集"] in await ui.rows()
        assert ["後手", "テスト後手"] in await ui.rows()
        await ui.button("Undo")
        assert not await ui.dirty()
        await ui.button("Redo")
        await ui.button("Redo")
        assert ["先手", "先手編集"] in await ui.rows()
        assert ["後手", "後手編集"] in await ui.rows()


async def test_cut_copy_paste_and_key_protection(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = GameInfoUi(session, tmp_path)
        await ui.open()
        rows = await ui.rows()
        black = next(i for i, row in enumerate(rows) if row[0] == "先手")
        white = next(i for i, row in enumerate(rows) if row[0] == "後手")
        await ui.call("click_table_cell", widget="gameInfoTable", row=black, column=1)
        await ui.button("Copy")
        assert (await ui.call("get_clipboard"))["text"] == "テスト先手"
        await ui.call("click_table_cell", widget="gameInfoTable", row=white, column=1)
        await ui.button("Paste")
        assert ["後手", "テスト先手"] in await ui.rows()
        await ui.button("Cut")
        assert ["後手", ""] in await ui.rows()
        await ui.button("Undo")
        assert ["後手", "テスト先手"] in await ui.rows()
        await ui.call("click_table_cell", widget="gameInfoTable", row=black, column=0)
        await ui.assert_button_disabled("Cut")
        await ui.assert_button_disabled("Paste")
        assert (await ui.rows())[black][0] == "先手"
        _, _, error = result_data(await session.call_tool("edit_table_cell", dict(widget="gameInfoTable", row=black, column=0, text="破損")))
        assert error


async def test_add_custom_row_and_round_trip(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = GameInfoUi(session, tmp_path)
        await ui.open()
        count = len(await ui.rows())
        await ui.button("AddRow")
        await ui.call("edit_table_cell", widget="gameInfoTable", row=count, column=0, text="備考")
        await ui.edit("備考", "追加した情報")
        await ui.button("Apply")
        path = tmp_path / "custom.jkf"
        await ui.call("save_kifu", path=str(path))
        await ui.call("load_kifu", path=str(path))
        assert ["備考", "追加した情報"] in await ui.rows()


async def test_active_editor_apply(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = GameInfoUi(session, tmp_path)
        await ui.open()
        await ui.edit("先手", "入力中の名前", commit=False)
        await ui.button("Apply")
        path = tmp_path / "active.kifu"
        await ui.call("save_kifu", path=str(path))
        assert "入力中の名前" in path.read_text(encoding="utf-8")


async def test_reset_cancel_preserves_pending_edits(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = GameInfoUi(session, tmp_path)
        await ui.open()
        await ui.edit("先手", "消えてはいけない名前")
        await ui.call("trigger_action", name="actionNewGame")
        box = await ui.message_box("未保存の変更")
        _, _, error = result_data(await session.call_tool("edit_table_cell", dict(
            widget="gameInfoTable", row=1, column=1, text="モーダル背後の編集")))
        assert error
        await ui.call("click_dialog_button", dialog=box, text="キャンセル")
        assert ["先手", "消えてはいけない名前"] in await ui.rows()


async def test_font_settings_survive_restart(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = GameInfoUi(session, tmp_path)
        await ui.open()
        original = (await ui.call("get_widget_text", widget="gameInfoTable"))["widgets"][0]["font_point_size"]
        await ui.button("FontIncrease")
        await ui.button("FontIncrease")
        await ui.button("FontDecrease")
    async with mcp_session(kifu_env) as session:
        ui = GameInfoUi(session, tmp_path)
        await ui.open()
        assert (await ui.call("get_widget_text", widget="gameInfoTable"))["widgets"][0]["font_point_size"] == original + 1


@pytest.mark.parametrize("extension", ["kif", "kifu", "ki2", "ki2u", "csa", "jkf"])
async def test_save_active_editor_round_trip(kifu_env, tmp_path, extension):
    async with mcp_session(kifu_env) as session:
        ui = GameInfoUi(session, tmp_path)
        await ui.open()
        await ui.edit("後手", "保存前の入力中", commit=False)
        assert (await ui.call("get_app_state"))["dirty"]
        _, _, error = result_data(await session.call_tool("load_kifu", {"path": str(FIXTURES / "test_basic.usi")}))
        assert error
        path = tmp_path / f"active.{extension}"
        await ui.call("save_kifu", path=str(path))
        assert not await ui.dirty()
        assert not (await ui.call("get_app_state"))["dirty"]
        await ui.call("load_kifu", path=str(path))
        assert ["後手", "保存前の入力中"] in await ui.rows()


async def test_saved_edits_can_be_undone_and_redone(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = GameInfoUi(session, tmp_path)
        await ui.open()
        await ui.assert_button_disabled("Apply")
        assert not (await ui.call("get_app_state"))["dirty"]
        await ui.edit("先手", "一度保存した名前")
        path = tmp_path / "saved.kifu"
        await ui.call("save_kifu", path=str(path))
        await ui.button("Undo")
        assert ["先手", "テスト先手"] in await ui.rows()
        assert (await ui.call("get_app_state"))["dirty"]
        await ui.button("Redo")
        assert not (await ui.call("get_app_state"))["dirty"]
        await ui.edit("後手", "再編集")
        await ui.button("Apply")
        _, _, error = result_data(await session.call_tool("save_kifu", {"path": str(path)}))
        assert error and (await ui.call("get_app_state"))["dirty"]
        await ui.call("load_kifu", path=str(path), discard_unsaved=True)
        await ui.assert_button_disabled("Undo")
        await ui.assert_button_disabled("Redo")
        assert ["後手", "テスト後手"] in await ui.rows()
        assert not (await ui.call("get_app_state"))["dirty"]


async def test_floating_dock_edits_survive_hide(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = GameInfoUi(session, tmp_path)
        await ui.open()
        await ui.call("configure_dock", widget="GameInfoDock", operation="float",
                      geometry=dict(x=50, y=50, width=800, height=500))
        rows = (await ui.call("get_widget_text", dialog="GameInfoDock", widget="gameInfoTable"))["widgets"][0]["rows"][1:]
        row = next(i for i, cells in enumerate(rows) if cells[0] == "先手")
        await ui.call("edit_table_cell", target="GameInfoDock", widget="gameInfoTable", row=row, column=1, text="浮動ウィンドウの編集")
        await ui.call("configure_dock", widget="GameInfoDock", operation="hide")
        assert (await ui.call("get_app_state"))["dirty"]
        await ui.call("show_dock", widget="GameInfoDock")
        await ui.call("configure_dock", widget="GameInfoDock", operation="dock", area="bottom")
        assert ["先手", "浮動ウィンドウの編集"] in await ui.rows()
        await ui.call("capture_screenshot", output_dir=str(tmp_path))


async def test_game_end_preserves_active_metadata(kifu_env, tmp_path):
    config = Path(kifu_env["XDG_CONFIG_HOME"]) / "ShogiBoardQ" / "ShogiBoardQ.ini"
    with config.open("a", encoding="utf-8") as ini:
        ini.write(f"[GameSettings]\nkifuSaveDir={tmp_path}\n")
    async with mcp_session(kifu_env) as session:
        ui = GameInfoUi(session, tmp_path)
        await ui.call("trigger_action", name="actionStartGame")
        await ui.dialog("StartGameDialog")
        for widget in ("comboBoxPlayer1", "comboBoxPlayer2"):
            await ui.call("set_widget_value", target="StartGameDialog", widget=widget, value=0)
        await ui.call("set_widget_value", target="StartGameDialog", widget="checkBoxAutoSaveKifu", value=True)
        await ui.call("click_dialog_button", dialog="StartGameDialog", text="対局開始")
        await ui.wait("get_app_state", lambda d: d["ui_state"] == "game")
        await ui.call("click_board_square", file=7, rank=7)
        await ui.call("click_board_square", file=7, rank=6)
        await ui.wait("get_app_state", lambda d: d["current_ply"] == 1)
        await ui.call("show_dock", widget="GameInfoDock")
        await ui.edit("先手", "終局直前の入力", commit=False)
        await ui.call("trigger_action", name="actionResign")
        await ui.call("close_dialog", dialog=await ui.message_box("投了"))
        state = await ui.wait("get_app_state", lambda d: bool(d["kifu_file"]))
        assert "終局直前の入力" in Path(state["kifu_file"]).read_text(encoding="utf-8")
        assert any(key == "終了日時" and value for key, value in await ui.rows())
        assert not state["dirty"]


async def test_discard_to_sfen_resets_metadata_history(kifu_env, tmp_path):
    async with mcp_session(kifu_env) as session:
        ui = GameInfoUi(session, tmp_path)
        await ui.open()
        await ui.edit("先手", "破棄する名前")
        await ui.call("set_position", sfen="lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1",
                      discard_unsaved=True)
        await ui.assert_button_disabled("Undo")
        await ui.assert_button_disabled("Redo")
        assert await ui.rows() == []
        assert not await ui.dirty()


@pytest.mark.parametrize("source", ["pasted_sfen", "handicap_record"])
async def test_current_position_game_keeps_record_start(kifu_env, tmp_path, source):
    # A pasted position or a loaded handicap record keeps its start position (and earlier moves)
    # when a game starts from the current position.
    async with mcp_session(kifu_env) as session:
        ui = GameInfoUi(session, tmp_path)
        if source == "pasted_sfen":
            start = "4k4/9/9/9/9/9/9/9/4K4 b G 1"
            await ui.call("set_position", sfen=start)
            ply, current = 0, start
        else:
            await ui.call("load_kifu", path=str(FIXTURES / "test_handicap_2piece.csa"))
            start = (await ui.call("goto_ply", ply=0))["sfen"]
            ply = 2
            current = (await ui.call("goto_ply", ply=ply))["sfen"]
        await ui.call("trigger_action", name="actionStartGame")
        await ui.dialog("対局")
        for widget in ("comboBoxPlayer1", "comboBoxPlayer2", "comboBoxStartingPosition"):
            await ui.call("set_widget_value", target="StartGameDialog", widget=widget, value=0)
        await ui.call("click_dialog_button", dialog="StartGameDialog", text="対局開始")
        state = await ui.wait("get_app_state", lambda d: d["ui_state"] == "game")
        assert (state["current_ply"], state["sfen"]) == (ply, current)
        await ui.call("trigger_action", name="actionBreakOffGame")
        state = await ui.wait("get_app_state", lambda d: d["ui_state"] == "idle" or d["dialogs"])
        for window in (await ui.call("list_dialogs"))["windows"]:
            if window["class"] == "QMessageBox":
                await ui.call("close_dialog", dialog=window["selector"])
        await ui.wait("get_app_state", lambda d: d["ui_state"] == "idle")
        assert (await ui.call("goto_ply", ply=0))["sfen"] == start
        assert (await ui.call("get_app_state"))["total_plies"] == ply + 1
