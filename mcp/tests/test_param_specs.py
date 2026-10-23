"""The MCP tool schemas come from the parameter tables of shogiboardq-cli and the automation API.

``shogiboardq-cli describe`` prints the C++ tables (CliCommandSpecs, AutomationMethodSpecs) as JSON Schema and
``shogiboardq_mcp/specs.json`` is that output saved with the package. The tools that pass their arguments on
build their input schemas from it (tooldefs._spec), so the only thing that can drift is the saved file.
"""
from __future__ import annotations

import json
import re
from pathlib import Path

import pytest

from shogiboardq_mcp import specs
from shogiboardq_mcp.tooldefs import ALL_TOOLS

PACKAGE_DIR = Path(__file__).resolve().parents[1] / "shogiboardq_mcp"

# Tools built on an operation: tool -> (kind, operation, parameters the tool does not expose).
BUILT_ON = {
    "convert_kifu": ("command", "convert-kifu", set()),
    "analyze_kifu": ("command", "analyze-kifu", {"stdin_control"}),
    "validate_sfen": ("command", "validate-sfen", set()),
    "render_board_image": ("command", "render-board", set()),
    "analyze_position": ("command", "analyze", {"stdin_control"}),
    "search_mate": ("command", "mate", {"stdin_control"}),
    "generate_tsume": ("command", "generate-tsume", {"stdin_control"}),
    "verify_tsume": ("command", "verify-tsume", set()),
    "set_position": ("method", "position.set", set()),
    "load_kifu": ("method", "kifu.load", set()),
    "save_kifu": ("method", "kifu.save", set()),
    "get_kifu": ("method", "kifu.get", set()),
    "goto_ply": ("method", "kifu.goto", set()),
    "trigger_action": ("method", "action.trigger", set()),
    "click_board_square": ("method", "board.click", set()),
    "click_dialog_button": ("method", "dialog.clickButton", set()),
    "show_dock": ("method", "widget.showDock", set()),
    "configure_dock": ("method", "dock.configure", set()),
    "click_widget": ("method", "widget.click", set()),
    "get_clipboard": ("method", "clipboard.get", set()),
    "edit_table_cell": ("method", "widget.editCell", set()),
    "set_widget_value": ("method", "widget.setValue", set()),
    "click_table_cell": ("method", "widget.clickCell", set()),
    "capture_screenshot": ("method", "screenshot.capture", set()),
    "close_dialog": ("method", "dialog.close", set()),
    "get_widget_text": ("method", "widget.text", set()),
    "list_menu_actions": ("method", "menu.items", set()),
    "select_menu_action": ("method", "menu.select", set()),
    "menu_favorites": ("method", "menu.favorites", set()),
    "click_branch_node": ("method", "branch.click", set()),
}
COMPARED = ("type", "enum", "default", "minimum", "maximum", "minItems", "maxItems", "pattern")


def test_specs_json_is_current(cli_path: Path):
    # After changing a C++ table, regenerate it: python3 -m shogiboardq_mcp.specs /path/to/shogiboardq-cli
    assert specs.dumps(specs.generate(str(cli_path))) == specs.SPECS_PATH.read_text(encoding="utf-8"), \
        "shogiboardq_mcp/specs.json is out of date"


def _tool(name: str) -> dict:
    return next(t for t in ALL_TOOLS if t.name == name).input_schema


@pytest.mark.parametrize("tool", sorted(BUILT_ON))
def test_tools_expose_the_operation_parameters(tool):
    kind, operation, dropped = BUILT_ON[tool]
    expected = specs.operation(kind, operation)
    schema = _tool(tool)
    for name, prop in expected["properties"].items():
        if name in dropped:
            assert name not in schema["properties"], f"{tool} exposes {name}"
            continue
        mine = schema["properties"][name]
        # A tool may narrow a range (verify_tsume waits for the call) but not change the type or the default
        for key in ("type", "enum", "default", "pattern"):
            assert mine.get(key) == prop.get(key), f"{tool}.{name} {key}"
        if "minimum" in prop:
            assert prop["minimum"] <= mine["minimum"] <= mine["maximum"] <= prop["maximum"], f"{tool}.{name} range"
    assert set(expected.get("required", [])) - dropped == set(schema.get("required", []))


def test_every_called_method_and_command_exists():
    # Automation methods called by the MCP handlers, and CLI commands they run, are in the tables.
    sources = "".join(p.read_text(encoding="utf-8") for p in PACKAGE_DIR.glob("handlers_*.py"))
    loaded = json.loads(specs.SPECS_PATH.read_text(encoding="utf-8"))
    for method in set(re.findall(r'client\.call(?:_queued)?\("([a-zA-Z]+\.[a-zA-Z]+)"', sources)):
        assert method in loaded["method"], method
    for command in set(re.findall(r'\bcommand\("([a-z-]+)"', sources)):
        assert command in loaded["command"], command
    # The job manager adds --stdin-control to the commands it runs as jobs
    for command in ("analyze", "analyze-kifu", "mate", "generate-tsume"):
        assert "stdin_control" in loaded["command"][command]["properties"], command
