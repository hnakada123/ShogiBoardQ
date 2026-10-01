"""Regression coverage for previously unreachable GUI features over real stdio MCP."""

from __future__ import annotations

import asyncio
from pathlib import Path

import pytest

from conftest import FIXTURES, mcp_session, result_data

pytestmark = pytest.mark.anyio


@pytest.fixture
def coverage_env(server_env, app_path, tmp_path):
    env = dict(server_env)
    config = tmp_path / "config" / "ShogiBoardQ"
    config.mkdir(parents=True)
    original = Path(env["XDG_CONFIG_HOME"]) / "ShogiBoardQ" / "ShogiBoardQ.ini"
    (config / "ShogiBoardQ.ini").write_text(
        original.read_text(encoding="utf-8") + "\n[%General]\nlanguage=ja_JP\n", encoding="utf-8")
    env.update(XDG_CONFIG_HOME=str(config.parent), SHOGIBOARDQ_EXECUTABLE=str(app_path),
               SHOGIBOARDQ_AUTOMATION_SOCKET=str(tmp_path / "app.sock"), SHOGIBOARDQ_QUIT_APP_ON_EXIT="1")
    return env


class UI:
    def __init__(self, session):
        self.session = session

    async def call(self, tool, **args):
        text, data, error = result_data(await self.session.call_tool(tool, args))
        assert not error, (tool, args, text)
        if data and (data.get("queued") or data.get("triggered")):
            await asyncio.sleep(0.05)
        return data

    async def error(self, tool, **args):
        text, _, error = result_data(await self.session.call_tool(tool, args))
        assert error, (tool, args, text)
        return text

    async def widgets(self, target="main", root=None):
        args = {"dialog": target, "max_rows": 200, "max_widgets": 1000}
        if root:
            args.update(widget=root, include_children=True)
        return (await self.call("get_widget_text", **args))["widgets"]

    async def find(self, target="main", root=None, **match):
        widgets = await self.widgets(target, root)
        matches = [w for w in widgets if w.get("visible", True) and all(w.get(k) == v for k, v in match.items())]
        assert matches, (target, root, match, widgets)
        return matches[0]

    async def read(self, widget, target="main"):
        return (await self.call("get_widget_text", dialog=target, widget=widget))["widgets"][0]

    async def click(self, text, target="main", root=None):
        widget = await self.find(target, root, text=text, enabled=True)
        await self.call("click_widget", target=target, widget=widget["selector"])

    async def rows(self, widget, count):
        for _ in range(100):
            data = await self.read(widget)
            if data["row_count"] == count:
                return data
            await asyncio.sleep(0.05)
        pytest.fail(f"{widget} did not reach {count} rows: {data}")

    async def dialog(self, class_name):
        for _ in range(100):
            windows = (await self.call("list_dialogs"))["windows"]
            match = [w for w in windows if w["class"] == class_name]
            if match:
                return match[0]["selector"]
            await asyncio.sleep(0.05)
        pytest.fail(f"Dialog {class_name} did not open: {windows}")

    async def open(self, action, class_name):
        await self.call("trigger_action", name=action)
        return await self.dialog(class_name)

    async def close(self, target):
        await self.call("close_dialog", dialog=target)
        await asyncio.sleep(0.05)

    async def file(self, path):
        dialog = await self.dialog("QFileDialog")
        await self.call("set_widget_value", target=dialog, widget="fileNameEdit", value=str(path), submit=True)

    async def input(self, value):
        dialog = await self.dialog("QInputDialog")
        edit = await self.find(dialog, **{"class": "QLineEdit"})
        assert edit["object_name"] == ""  # The original gap: no objectName.
        await self.call("set_widget_value", target=dialog, widget=edit["selector"], value=value)
        await self.click("OK", dialog)


async def test_colors_sound_groups_and_language(coverage_env):
    async with mcp_session(coverage_env) as session:
        ui = UI(session)
        color = await ui.open("actionBoardAppearance", "BoardColorDialog")
        await ui.call("set_widget_value", target=color, widget="appearanceSections", value=5)
        await ui.call("set_widget_value", target=color, widget="boardColorTabs", value=5)
        await ui.call("set_widget_value", target=color, widget="boardThemeCombo", value="墨と榧")
        await ui.call("set_widget_value", target=color, widget="boardPieceScale", value=94)
        assert (await ui.read("boardPieceScale", color))["value"] == 94
        await ui.call("set_widget_value", target=color, widget="boardColorTabs", value=0)
        await ui.call("set_widget_value", target=color, widget="boardColorPresetCombo", value="生成り")
        assert (await ui.read("boardColorButton", color))["text"] == "#D9CFBB"
        await ui.call("click_widget", target=color, widget="boardColorButton")
        picker = await ui.dialog("QColorDialog")
        await ui.call("set_widget_value", target=picker, widget="qt_colorname_lineedit", value="#123456")
        await ui.click("OK", picker)
        assert (await ui.read("boardColorButton", color))["text"] == "#123456"
        await ui.call("click_widget", target=color, widget="boardColorButton")
        picker = await ui.dialog("QColorDialog")
        await ui.error("set_widget_value", target=picker, widget="boardColorPicker", value="bad-color")
        await ui.call("set_widget_value", target=picker, widget="boardColorPicker", value="#654321")
        await ui.click("OK", picker)
        assert (await ui.read("boardColorButton", color))["text"] == "#654321"
        await ui.close(color)

        sound = await ui.open("actionPieceSoundSettings", "PieceSoundSettingsDialog")
        for widget, value in (("pieceSoundVolume", 25), ("pieceSoundPitch", 3), ("pieceSoundLow", -2),
                              ("pieceSoundMid", 4), ("pieceSoundHigh", -3)):
            await ui.call("set_widget_value", target=sound, widget=widget, value=value)
            assert (await ui.read(widget, sound))["value"] == value
        await ui.error("set_widget_value", target=sound, widget="pieceSoundVolume", value=101)
        await ui.click("OK", sound)

        game = await ui.open("actionStartGame", "StartGameDialog")
        await ui.call("set_widget_value", target=game, widget="groupBoxSecondPlayerTimeSettings", value=True)
        await ui.call("set_widget_value", target=game, widget="byoyomiSec2", value=17)
        assert (await ui.read("groupBoxSecondPlayerTimeSettings", game))["checked"]
        assert (await ui.read("byoyomiSec2", game))["value"] == 17
        await ui.call("set_widget_value", target=game, widget="groupBoxSecondPlayerTimeSettings", value=False)
        await ui.error("set_widget_value", target=game, widget="byoyomiSec2", value=19)
        await ui.close(game)
        await ui.call("trigger_action", name="actionLanguageEnglish")
        await ui.close(await ui.dialog("QMessageBox"))
    async with mcp_session(coverage_env) as session:
        ui = UI(session)
        actions = (await ui.call("list_actions"))["actions"]
        assert next(a for a in actions if a["name"] == "actionLanguageEnglish")["checked"]
        sound = await ui.open("actionPieceSoundSettings", "PieceSoundSettingsDialog")
        assert (await ui.read("pieceSoundVolume", sound))["value"] == 25
        await ui.close(sound)


async def test_record_comments_bookmarks_and_branch_nodes(coverage_env, tmp_path):
    async with mcp_session(coverage_env) as session:
        ui = UI(session)
        await ui.call("load_kifu", path=str(FIXTURES / "test_comments.kif"))
        await ui.call("goto_ply", ply=3)
        await ui.call("show_dock", widget="CommentDock")
        await ui.call("set_widget_value", widget="kifuCommentEdit", value="MCPから編集したコメント")
        await ui.call("click_widget", widget="kifuCommentApply")
        moves = (await ui.call("get_kifu"))["moves"]
        assert moves[2]["comment"] == "MCPから編集したコメント"
        await ui.call("click_widget", widget="kifuBookmarkEdit")
        await ui.input("検証しおり")
        for column in ("kifuToggleTime", "kifuToggleBookmark", "kifuToggleComment"):
            await ui.call("set_widget_value", widget=column, value=True)
            assert (await ui.read(column))["checked"]
        assert "検証しおり" in (await ui.read("kifuTable"))["rows"][4]
        saved = tmp_path / "annotated.kif"
        await ui.call("save_kifu", path=str(saved))
        text = (await ui.call("get_kifu", format="kif"))["text"]
        assert "検証しおり" in text and "MCPから編集したコメント" in text

        await ui.call("load_kifu", path=str(FIXTURES / "test_branch.kif"), discard_unsaved=True)
        await ui.call("show_dock", widget="BranchTreeDock")
        nodes = (await ui.read("branchTreeView"))["nodes"]
        node = next(n for n in nodes if n["row"] == 1 and n["ply"] == 3)
        await ui.call("click_branch_node", widget="branchTreeView", id=node["id"])
        pos = await ui.call("get_position")
        assert pos["ply"] == 3 and pos["moves"][-1] == "6g6f"
        await ui.error("click_branch_node", widget="branchTreeView", id=999999)


async def test_menu_customization_and_saved_layouts(coverage_env):
    async with mcp_session(coverage_env) as session:
        ui = UI(session)
        await ui.call("get_app_state")
        await ui.call("show_dock", widget="MenuWindowDock")
        await ui.call("set_widget_value", widget="menuTabs", value=1)
        await ui.call("set_widget_value", widget="menuCustomize", value=True)
        toolbar = await ui.widgets(root="toolBar")
        save = next(w for w in toolbar if w.get("action_name") == "actionSave")
        assert "not_allowed" in await ui.error("click_widget", widget=save["selector"])
        await ui.call("menu_favorites", actions=["actionQuit"])
        quit_button = next(w for w in await ui.widgets(root="MenuWindowDock")
                           if w.get("action_name") == "actionQuit")
        assert "not_allowed" in await ui.error("click_widget", widget=quit_button["selector"])
        for name in ("menuButtonSizeIncrease", "menuButtonSizeDecrease", "menuFontSizeIncrease", "menuFontSizeDecrease"):
            await ui.call("click_widget", widget=name)
        await ui.call("menu_favorites", actions=["actionFlipBoard", "actionBoardAppearance"])
        await ui.call("menu_favorites", actions=["actionBoardAppearance", "actionFlipBoard"])
        await ui.call("menu_favorites", actions=["actionBoardAppearance"])
        assert (await ui.call("menu_favorites"))["actions"] == ["actionBoardAppearance"]
        await ui.error("menu_favorites", actions=["missing"])

        await ui.call("configure_dock", widget="RecordPaneDock", operation="float")
        await ui.call("trigger_action", name="actionSaveDockLayout")
        await ui.input("MCP配置")
        await ui.close(await ui.dialog("QMessageBox"))
        await ui.call("trigger_action", name="actionResetDockLayout")
        menu = await ui.call("list_menu_actions", widget="menuSavedLayouts")
        layout = next(item for item in menu["items"] if "MCP配置" in item["text"])
        restore = next(item for item in layout["items"] if item["text"] == "復元")
        await ui.call("select_menu_action", widget="menuSavedLayouts", path=[layout["index"], restore["index"]])
        docks = (await ui.call("list_docks"))["docks"]
        assert next(d for d in docks if d["object_name"] == "RecordPaneDock")["floating"]
        startup = next(item for item in layout["items"] if "起動時" in item["text"])
        await ui.call("select_menu_action", widget="menuSavedLayouts", path=[layout["index"], startup["index"]])
        await ui.close(await ui.dialog("QMessageBox"))
    async with mcp_session(coverage_env) as session:
        ui = UI(session)
        assert (await ui.call("menu_favorites"))["actions"] == ["actionBoardAppearance"]
        items = (await ui.call("list_menu_actions", widget="menuSavedLayouts"))["items"]
        layout = next(item for item in items if "MCP配置" in item["text"])
        assert "★" in layout["text"]
        delete = next(item for item in layout["items"] if item["text"] == "削除")
        await ui.call("select_menu_action", widget="menuSavedLayouts", path=[layout["index"], delete["index"]])
        question = await ui.dialog("QMessageBox")
        # Qt's native standard button text varies with the installed Qt translations.
        buttons = await ui.widgets(question)
        yes = next(w for w in buttons if w["class"] == "QPushButton" and w.get("text") in ("&Yes", "Yes", "はい(&Y)"))
        await ui.call("click_widget", target=question, widget=yes["selector"])
        assert all("MCP配置" not in i["text"] for i in (await ui.call("list_menu_actions", widget="menuSavedLayouts"))["items"])


async def test_engine_list_selection_and_settings(coverage_env):
    async with mcp_session(coverage_env) as session:
        ui = UI(session)
        target = await ui.open("actionEngineSettings", "EngineRegistrationDialog")
        initial = (await ui.read("engineListWidget", target))["items"]
        if not initial:
            pytest.skip("A test engine is required")
        await ui.call("set_widget_value", target=target, widget="engineListWidget", value=0)
        assert (await ui.read("engineListWidget", target))["current_index"] == 0
        await ui.call("click_widget", target=target, widget="configureEngineButton")
        settings = await ui.dialog("ChangeEngineSettingsDialog")
        await ui.call("set_widget_value", target=settings, widget="ponderEnabledCheckBox", value=True)
        await ui.close(settings)
        await ui.call("click_widget", target=target, widget="removeEngineButton")
        assert len((await ui.read("engineListWidget", target))["items"]) == len(initial) - 1
        await ui.close(target)


async def test_joseki_add_edit_save_delete_and_merge(coverage_env, tmp_path):
    async with mcp_session(coverage_env) as session:
        ui = UI(session)
        await ui.call("get_app_state")
        await ui.call("show_dock", widget="JosekiWindowDock")
        await ui.click("＋追加", root="JosekiWindowDock")
        dialog = await ui.dialog("JosekiMoveDialog")
        combos = [w for w in await ui.widgets(dialog) if w["class"] == "QComboBox"]
        for combo, value in zip(combos[:4], (6, 6, 6, 5), strict=True):
            await ui.call("set_widget_value", target=dialog, widget=combo["selector"], value=value)
        await ui.click("追加", dialog)
        assert (await ui.read("josekiTable"))["row_count"] == 1
        await ui.click("編集", root="JosekiWindowDock")
        dialog = await ui.dialog("JosekiMoveDialog")
        edit = next(w for w in await ui.widgets(dialog) if w["class"] == "QLineEdit" and w.get("placeholder", "").startswith("コメント"))
        await ui.call("set_widget_value", target=dialog, widget=edit["selector"], value="MCP定跡")
        await ui.click("更新", dialog)
        assert "MCP定跡" in (await ui.read("josekiTable"))["rows"][1]
        await ui.click("別名保存", root="JosekiWindowDock")
        book = tmp_path / "opening.db"
        await ui.file(book)
        for _ in range(100):
            if book.exists():
                break
            await asyncio.sleep(0.05)
        assert "7g7f" in book.read_text()
        await ui.click("削除", root="JosekiWindowDock")
        question = await ui.dialog("QMessageBox")
        yes = next(w for w in await ui.widgets(question) if w["class"] == "QPushButton" and w.get("text") in ("&Yes", "Yes", "はい(&Y)"))
        await ui.call("click_widget", target=question, widget=yes["selector"])
        assert (await ui.read("josekiTable"))["row_count"] == 0
        await ui.click("保存", root="JosekiWindowDock")
        await asyncio.sleep(0.2)
        await ui.call("load_kifu", text="position startpos moves 7g7f 3c3d")
        await ui.call("select_menu_action", widget="josekiMergeMenu", path=[0])
        merge = await ui.dialog("JosekiMergeDialog")
        await ui.click("全て登録", merge)
        complete = await ui.dialog("QMessageBox")
        await ui.error("close_dialog", dialog=merge)
        await ui.close(complete)
        await ui.close(merge)
        await ui.call("goto_ply", ply=0)
        await ui.call("show_dock", widget="JosekiWindowDock")
        assert (await ui.read("josekiTable"))["row_count"] == 1
        assert "3c3d" in book.read_text()
        await ui.click("新規", root="JosekiWindowDock")
        await ui.rows("josekiTable", 0)
        await ui.click("開く", root="JosekiWindowDock")
        await ui.file(book)
        await ui.rows("josekiTable", 1)
        await ui.click("新規", root="JosekiWindowDock")
        items = (await ui.call("list_menu_actions", widget="josekiRecentMenu"))["items"]
        recent = next(item for item in items if "opening.db" in item["text"])
        await ui.call("select_menu_action", widget="josekiRecentMenu", path=[recent["index"]])
        await ui.rows("josekiTable", 1)
        await ui.call("save_kifu", path=str(tmp_path / "game.kif"))


async def test_collection_recent_menu(coverage_env, tmp_path):
    async with mcp_session(coverage_env) as session:
        ui = UI(session)
        record = await ui.call("convert_kifu", text="position startpos moves 7g7f", output_format="sfen")
        path = tmp_path / "positions.sfen"
        path.write_text("\n".join(record["sfens"]) + "\n", encoding="utf-8")
        dialog = await ui.open("actionSfenCollectionViewer", "SfenCollectionDialog")
        await ui.click("ファイルを開く", dialog)
        await ui.file(path)
        await ui.click("次へ ▶", dialog)
        board = await ui.find(dialog, **{"class": "ShogiView"})
        assert board["board_sfen"] == record["sfens"][1].split()[0]
        await ui.close(dialog)
        dialog = await ui.open("actionSfenCollectionViewer", "SfenCollectionDialog")
        items = (await ui.call("list_menu_actions", target=dialog, widget="sfenCollectionRecentMenu"))["items"]
        recent = next(item for item in items if "positions.sfen" in item["text"])
        await ui.call("select_menu_action", target=dialog, widget="sfenCollectionRecentMenu", path=[recent["index"]])
        board = await ui.find(dialog, **{"class": "ShogiView"})
        assert board["board_sfen"] == record["sfens"][0].split()[0]
        button = await ui.find(dialog, text="閉じる")
        await ui.call("click_dialog_button", dialog=dialog, widget=button["selector"])


async def test_analysis_results_gui_and_batch_job(coverage_env, tmp_path):
    async with mcp_session(coverage_env) as session:
        ui = UI(session)
        engines = (await ui.call("list_engines"))["engines"]
        if not any(e["name"] == "TestUsi" for e in engines):
            pytest.skip("TestUsi is required")
        await ui.call("load_kifu", text="position startpos moves 7g7f 3c3d")
        dialog = await ui.open("actionAnalyzeKifu", "KifuAnalysisDialog")
        await ui.call("set_widget_value", target=dialog, widget="comboBoxEngine1", value="TestUsi")
        await ui.call("set_widget_value", target=dialog, widget="byoyomiSec", value=1)
        await ui.click("OK", dialog)
        await ui.close(await ui.dialog("QMessageBox"))
        for _ in range(300):
            state = await ui.call("get_app_state")
            if state["ui_state"] == "idle":
                break
            await asyncio.sleep(0.05)
        await ui.call("show_dock", widget="AnalysisResultsDock")
        assert (await ui.read("analysisResultsTable"))["row_count"] >= 1
        await ui.call("click_table_cell", widget="analysisResultsTable", row=0, column=6)
        windows = (await ui.call("list_dialogs"))["windows"]
        pv = next(w for w in windows if w["class"] == "PvBoardDialog")
        await ui.close(pv["selector"])
        job = await ui.call("analyze_kifu", engine="TestUsi", text="position startpos moves 7g7f 3c3d",
                            seconds_per_position=1)
        for _ in range(300):
            result = await ui.call("kifu_analysis_status", job_id=job["job_id"], max_positions=1)
            if result["state"] not in ("running", "stopping"):
                break
            await asyncio.sleep(0.05)
        assert result["state"] == "finished" and result["completed"] == 3
        assert result["truncated"] and result["next_offset"] == 1
        remaining = await ui.call("kifu_analysis_result", job_id=job["job_id"], offset=1)
        assert [p["ply"] for p in remaining["positions"]] == [1, 2]
        first = remaining["positions"][0]
        assert first["played_move"] == "7g7f" and first["score_cp_black"] == -first["lines"][0]["score_cp"]
        assert not remaining["partial"] and not remaining["truncated"]
        job = await ui.call("analyze_kifu", engine="TestUsi", text="position startpos moves 7g7f 3c3d", seconds_per_position=30)
        await ui.call("cancel_job", job_id=job["job_id"])
        result = await ui.call("kifu_analysis_result", job_id=job["job_id"])
        assert result["state"] == "stopped" and result["partial"] and result["completed"] < 3
        await ui.error("analyze_kifu", engine="TestUsi", text="position startpos moves 7g7f", from_ply=10)
        await ui.call("save_kifu", path=str(tmp_path / "analyzed.kif"))
