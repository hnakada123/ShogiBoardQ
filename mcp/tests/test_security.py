"""Security boundaries, including the GUI bypass reproduced during the audit."""
from __future__ import annotations

import asyncio

import pytest

from shogiboardq_mcp.app_client import AppClient, AppError
from shogiboardq_mcp.errors import ToolError
from shogiboardq_mcp.server import require_unrestricted_ui, with_defaults
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
])
async def test_restricted_gui_rejects_before_connecting(monkeypatch, tool, args):
    monkeypatch.delenv("SHOGIBOARDQ_ALLOW_UNRESTRICTED_UI", raising=False)
    # The server checks the declared tools before calling any handler (no RPC or auto-launch).
    assert tool in UNRESTRICTED_TOOLS
    assert set(with_defaults(tool, args)) <= set(next(t for t in ALL_TOOLS if t.name == tool).input_schema["properties"])
    with pytest.raises(ToolError) as error:
        require_unrestricted_ui()
    assert error.value.code == "not_allowed"


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
