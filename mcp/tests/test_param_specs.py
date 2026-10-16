"""The MCP tool schemas agree with the parameter tables of shogiboardq-cli and the automation API.

``shogiboardq-cli describe`` prints the C++ tables (CliCommandSpecs, AutomationMethodSpecs) as JSON Schema.
Each MCP parameter that is passed on to a CLI option or an automation parameter must have the same type,
choices, default and range there, so the three layers cannot drift apart again.
"""
from __future__ import annotations

import json
import os
import re
import subprocess
from pathlib import Path

import pytest

from shogiboardq_mcp.tooldefs import ALL_TOOLS

PACKAGE_DIR = Path(__file__).resolve().parents[1] / "shogiboardq_mcp"
COMPARED = ("type", "enum", "default", "minimum", "maximum", "minItems", "maxItems")

# MCP tools whose arguments are sent unchanged to an automation method.
PASS_THROUGH = {
    "set_position": "position.set",
    "load_kifu": "kifu.load",
    "goto_ply": "kifu.goto",
    "trigger_action": "action.trigger",
    "click_board_square": "board.click",
    "click_dialog_button": "dialog.clickButton",
    "show_dock": "widget.showDock",
    "configure_dock": "dock.configure",
    "click_widget": "widget.click",
    "get_clipboard": "clipboard.get",
    "edit_table_cell": "widget.editCell",
    "set_widget_value": "widget.setValue",
    "click_table_cell": "widget.clickCell",
    "capture_screenshot": "screenshot.capture",
    "close_dialog": "dialog.close",
    "get_widget_text": "widget.text",
    "list_menu_actions": "menu.items",
    "select_menu_action": "menu.select",
    "menu_favorites": "menu.favorites",
    "click_branch_node": "branch.click",
}

# MCP tools built on another operation: MCP parameter -> (operation parameter, how they relate).
# "same" compares every attribute, "invert" is a boolean flag with the opposite default,
# ("scale", k) multiplies values by k (the MCP range may be narrower), "range" compares the range only.
MAPPED = {
    ("method", "save_kifu", "kifu.save"): {"path": ("path", "type"), "overwrite": ("overwrite", "same")},
    ("method", "get_kifu", "kifu.get"): {p: (p, "same") for p in ("format", "line", "from_ply", "max_moves")},
    ("command", "validate_sfen", "validate-sfen"): {"sfen": ("sfen", "type")},
    ("command", "convert_kifu", "convert-kifu"): {
        "input_path": ("input", "type"), "text": ("text", "type"), "input_format": ("input_format", "same"),
        "output_format": ("output_format", "same"), "output_path": ("output", "type"), "overwrite": ("overwrite", "same")},
    ("command", "render_board_image", "render-board"): {
        "sfen": ("sfen", "same"), "output_path": ("output", "type"), "square_size": ("square_size", "same"),
        "flip": ("flip", "same"), "last_move": ("last_move", "type"), "black_name": ("black_name", "type"),
        "white_name": ("white_name", "type"), "overwrite": ("overwrite", "same")},
    ("command", "analyze_position", "analyze"): {
        "engine": ("engine", "type"), "sfen": ("sfen", "same"), "moves": ("moves", "same"),
        "seconds": ("seconds", "same"), "multipv": ("multipv", "same")},
    ("command", "search_mate", "mate"): {
        "engine": ("engine", "type"), "sfen": ("sfen", "same"), "moves": ("moves", "same"), "seconds": ("seconds", "same")},
    ("command", "generate_tsume", "generate-tsume"): {
        "engine": ("engine", "type"), "target_moves": ("target_moves", "same"), "max_positions": ("max_positions", "same"),
        "timeout_ms": ("timeout_ms", "same"), "max_attack_pieces": ("max_attack", "same"),
        "max_defend_pieces": ("max_defend", "same"), "attack_range": ("attack_range", "same"),
        "add_remaining_to_defender_hand": ("no_remaining_to_hand", "invert"),
        "allow_final_move_alternatives": ("no_final_alternatives", "invert")},
    ("command", "verify_tsume", "verify-tsume"): {
        "engine": ("engine", "type"), "sfen": ("sfen", "same"), "target_moves": ("target_moves", "same"),
        "timeout_seconds": ("timeout_ms", ("scale", 1000)), "allow_final_move_alternatives": ("no_final_alternatives", "invert")},
    ("command", "analyze_kifu", "analyze"): {
        "engine": ("engine", "type"), "seconds_per_position": ("seconds", "range"), "multipv": ("multipv", "same")},
}


@pytest.fixture(scope="module")
def described(cli_path: Path) -> dict:
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen")
    out = subprocess.run([str(cli_path), "describe"], capture_output=True, text=True, timeout=60, env=env, check=True)
    data = json.loads(out.stdout)
    assert data["ok"]
    return {"command": {c["name"]: c["input_schema"] for c in data["commands"]},
            "method": {m["name"]: m["input_schema"] for m in data["methods"]}}


def _tool(name: str) -> dict:
    return next(t for t in ALL_TOOLS if t.name == name).input_schema


def _attributes(prop: dict) -> dict:
    attrs = {key: prop[key] for key in COMPARED if key in prop}
    if attrs.get("type") == "array":
        attrs.setdefault("minItems", 0)  # an array without minItems may be empty
    return attrs


@pytest.mark.parametrize("tool,method", sorted(PASS_THROUGH.items()))
def test_pass_through_tools_match_automation_methods(described, tool, method):
    mcp_schema = _tool(tool)
    app_schema = described["method"][method]
    assert set(mcp_schema["properties"]) == set(app_schema["properties"]), "parameter names differ"
    assert set(mcp_schema.get("required", [])) == set(app_schema.get("required", []))
    for name, prop in mcp_schema["properties"].items():
        assert _attributes(prop) == _attributes(app_schema["properties"][name]), f"{tool}.{name} vs {method}.{name}"


@pytest.mark.parametrize("key", sorted(MAPPED, key=lambda k: k[1]), ids=lambda k: k[1])
def test_mapped_parameters_match(described, key):
    kind, tool, operation = key
    mcp_props = _tool(tool)["properties"]
    target_props = described[kind][operation]["properties"]
    for name, (target_name, relation) in MAPPED[key].items():
        mine, theirs = mcp_props[name], target_props[target_name]
        label = f"{tool}.{name} vs {operation} {target_name}"
        if relation == "same":
            assert _attributes(mine) == _attributes(theirs), label
        elif relation == "type":
            assert mine["type"] == theirs["type"], label
        elif relation == "invert":
            assert mine["type"] == theirs["type"] == "boolean", label
            assert mine.get("default") is (not theirs.get("default", False)), label
        elif relation == "range":
            assert (mine["minimum"], mine["maximum"]) == (theirs["minimum"], theirs["maximum"]), label
        else:
            factor = relation[1]
            assert mine.get("default", 0) * factor == theirs.get("default", 0), label
            assert theirs["minimum"] <= mine["minimum"] * factor <= mine["maximum"] * factor <= theirs["maximum"], label


def test_every_called_method_and_option_exists(described):
    # Automation methods called by the MCP handlers, and CLI commands they run, are in the tables.
    sources = "".join(p.read_text(encoding="utf-8") for p in PACKAGE_DIR.glob("handlers_*.py"))
    for method in set(re.findall(r'client\.call\("([a-zA-Z]+\.[a-zA-Z]+)"', sources)):
        assert method in described["method"], method
    for command, options in re.findall(r'\[\s*"([a-z-]+)",((?:\s*"--[a-z-]+"[^\]]*?)*)', sources):
        if command not in described["command"]:
            continue
        known = {"--" + n.replace("_", "-") for n in described["command"][command]["properties"]}
        for option in re.findall(r'"(--[a-z-]+)"', options):
            assert option in known, f"{command} {option}"
    assert {k[2] for k in MAPPED if k[0] == "command"} <= set(described["command"])
