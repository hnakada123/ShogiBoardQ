"""Security boundaries, including the GUI bypass reproduced during the audit."""
from __future__ import annotations

import asyncio

import pytest

from shogiboardq_mcp.app_client import AppClient, AppError
from shogiboardq_mcp.errors import ToolError
from shogiboardq_mcp.server import check_arguments, require_unrestricted_ui, with_defaults
from shogiboardq_mcp.tooldefs import ALL_TOOLS, UNRESTRICTED_TOOLS

pytestmark = pytest.mark.anyio


@pytest.mark.parametrize("tool,args", [
    ("trigger_action", {"name": "actionOpenKifuFile"}),
    ("trigger_action", {"name": "actionSaveAs"}),
    ("set_widget_value", {"target": "QFileDialog", "widget": "fileNameEdit", "value": "/outside.kif"}),
    ("click_widget", {"widget": "openButton"}),
    ("click_dialog_button", {"target": "QFileDialog", "text": "Open"}),
    ("click_table_cell", {"widget": "table", "row": 0, "column": 0}),
    ("edit_table_cell", {"widget": "table", "row": 0, "column": 0, "text": "x"}),
    ("select_menu_action", {"widget": "recentFiles", "path": [0]}),
    # The clipboard holds what the user copied in other applications (passwords included).
    ("get_clipboard", {}),
])
async def test_restricted_gui_rejects_before_connecting(monkeypatch, tool, args):
    monkeypatch.delenv("SHOGIBOARDQ_ALLOW_UNRESTRICTED_UI", raising=False)
    # The server checks the declared tools before calling any handler (no RPC or auto-launch).
    assert tool in UNRESTRICTED_TOOLS
    assert set(with_defaults(tool, args)) <= set(next(t for t in ALL_TOOLS if t.name == tool).input_schema["properties"])
    with pytest.raises(ToolError) as error:
        require_unrestricted_ui()
    assert error.value.code == "not_allowed"


@pytest.mark.parametrize("tool,args,where", [
    ("validate_sfen", {"sfen": "startpos", "bogus_key": 1}, "bogus_key"),
    ("convert_kifu", {"input_path": "", "output_format": "kif"}, "input_path"),
    ("convert_kifu", {"text": "x", "output_format": "kif", "max_chars": 10**9}, "max_chars"),
    ("verify_tsume", {"engine": "e", "sfen": "startpos", "target_moves": 1, "timeout_seconds": 120}, "timeout_seconds"),
])
async def test_arguments_are_checked_against_the_input_schema(tool, args, where):
    # The lowlevel mcp Server does not validate arguments, so the server checks them before any handler runs.
    with pytest.raises(ToolError) as error:
        check_arguments(tool, args)
    assert error.value.code == "invalid_argument"
    assert where in error.value.message
    check_arguments("validate_sfen", {"sfen": "startpos"})


async def test_engine_executable_cannot_be_chosen(monkeypatch, app_path, tmp_path):
    # Even with full GUI control, adding an engine (which runs the chosen file) is refused.
    for key, value in {
        "SHOGIBOARDQ_EXECUTABLE": str(app_path),
        "SHOGIBOARDQ_AUTOMATION_SOCKET": str(tmp_path / "a.sock"),
        "SHOGIBOARDQ_CONFIG_HOME": str(tmp_path / "config"),
        "SHOGIBOARDQ_DATA_HOME": str(tmp_path / "data"),
        "SHOGIBOARDQ_QUIT_APP_ON_EXIT": "1",
        "QT_QPA_PLATFORM": "offscreen",
    }.items():
        monkeypatch.setenv(key, value)
    client = AppClient()
    try:
        await client.call("action.trigger", {"name": "actionEngineSettings"})
        target = None
        for _ in range(50):
            windows = (await client.call("dialog.list"))["windows"]
            target = next((w["selector"] for w in windows if w["class"] == "EngineRegistrationDialog"), None)
            if target:
                break
            await asyncio.sleep(0.02)
        assert target
        for method in ("widget.click", "dialog.clickButton"):
            with pytest.raises(AppError) as error:
                await client.call(method, {"target": target, "widget": "addEngineButton"})
            assert error.value.code == "not_allowed"
        assert not any(w["class"] == "QFileDialog" for w in (await client.call("dialog.list"))["windows"])
        await client.call("dialog.close", {"target": target})
    finally:
        await client.shutdown()


async def test_kifu_save_refuses_other_files(monkeypatch, app_path, tmp_path):
    # The application itself saves records only under record extensions and never through links.
    for key, value in {
        "SHOGIBOARDQ_EXECUTABLE": str(app_path),
        "SHOGIBOARDQ_AUTOMATION_SOCKET": str(tmp_path / "a.sock"),
        "SHOGIBOARDQ_CONFIG_HOME": str(tmp_path / "config"),
        "SHOGIBOARDQ_DATA_HOME": str(tmp_path / "data"),
        "SHOGIBOARDQ_QUIT_APP_ON_EXIT": "1",
        "QT_QPA_PLATFORM": "offscreen",
    }.items():
        monkeypatch.setenv(key, value)
    client = AppClient()
    try:
        for name in ("notes.txt", ".bashrc", "noext"):
            with pytest.raises(AppError) as error:
                await client.call("kifu.save", {"path": str(tmp_path / name)})
            assert error.value.code == "invalid_argument"
            assert not (tmp_path / name).exists()
        link = tmp_path / "link.kif"
        try:
            link.symlink_to(tmp_path / "elsewhere.kif")
        except OSError:
            link = None
        if link is not None:
            with pytest.raises(AppError) as error:
                await client.call("kifu.save", {"path": str(link), "overwrite": True})
            assert error.value.code == "invalid_argument"
            assert not (tmp_path / "elsewhere.kif").exists()
        saved = await client.call("kifu.save", {"path": str(tmp_path / "game.kifu")})
        assert saved["format"] == "kifu"
    finally:
        await client.shutdown()


async def test_protected_password_cannot_be_revealed(monkeypatch, app_path, tmp_path):
    # Exercise the application boundary even when full GUI control is granted.
    for key, value in {
        "SHOGIBOARDQ_EXECUTABLE": str(app_path),
        "SHOGIBOARDQ_AUTOMATION_SOCKET": str(tmp_path / "a.sock"),
        "SHOGIBOARDQ_CONFIG_HOME": str(tmp_path / "config"),
        "SHOGIBOARDQ_DATA_HOME": str(tmp_path / "data"),
        "SHOGIBOARDQ_QUIT_APP_ON_EXIT": "1",
        "QT_QPA_PLATFORM": "offscreen",
    }.items():
        monkeypatch.setenv(key, value)
    client = AppClient()
    try:
        await client.call("action.trigger", {"name": "actionCSA"})
        target = None
        for _ in range(50):
            windows = (await client.call("dialog.list"))["windows"]
            target = next((w["selector"] for w in windows if w["class"] == "CsaGameDialog"), None)
            if target:
                break
            await asyncio.sleep(0.02)
        assert target
        await client.call("widget.setValue", {"target": target, "widget": "lineEditPassword", "value": "audit-secret"})
        await asyncio.sleep(0.05)
        result = await client.call("widget.text", {"target": target, "widget": "lineEditPassword"})
        assert result["widgets"][0]["text"] == "*****"
        for method, params in [
            ("widget.setValue", {"target": target, "widget": "checkBoxShowPassword", "value": True}),
            ("widget.click", {"target": target, "widget": "checkBoxShowPassword"}),
            ("dialog.clickButton", {"target": target, "widget": "checkBoxShowPassword"}),
        ]:
            with pytest.raises(AppError) as error:
                await client.call(method, params)
            assert error.value.code == "not_allowed"
        assert "audit-secret" not in str(await client.call("widget.text", {"target": target}))
        await client.call("dialog.close", {"target": target})
    finally:
        await client.shutdown()
