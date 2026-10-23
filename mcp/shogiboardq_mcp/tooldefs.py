"""Tool definitions: names, English descriptions, strict JSON Schemas, annotations.

Phase 1 tools run through ``shogiboardq-cli`` and need no GUI. Phase 2 tools
talk to a running ``ShogiBoardQ --automation`` instance.
"""

from __future__ import annotations

from typing import Any

import mcp.types as types

from . import specs

# Tools that pass their arguments on take the input schema from the C++ tables (specs.json, generated from
# `shogiboardq-cli describe`): the CLI commands and automation methods have the same parameter names as the tools.
# Descriptions there name automation methods and CLI commands; the tools show the MCP tool names instead.
_OPERATION_TOOL_NAMES = {
    "widget.text": "get_widget_text",
    "widget.click": "click_widget",
    "widget.setValue": "set_widget_value",
    "menu.items": "list_menu_actions",
    "action.list": "list_actions",
    "list-engines": "list_engines",
}


def _spec(kind: str, name: str, **changes: Any) -> dict[str, Any]:
    """Input schema of the CLI command (kind "command") or automation method (kind "method") a tool calls."""
    schema = specs.schema(kind, name, **changes)
    for prop in schema["properties"].values():
        if "description" in prop:
            for operation, tool in _OPERATION_TOOL_NAMES.items():
                prop["description"] = prop["description"].replace(operation, tool)
    return schema


JOB_ID = {"type": "string", "minLength": 1, "description": "Job id returned by the tool that started the job."}
ABS_PATH_DESC = "Absolute path inside an allowed directory (default: the user's home directory)."
MAX_PV_MOVES = {
    "type": "integer",
    "minimum": 1,
    "maximum": 200,
    "default": 12,
    "description": "Maximum number of moves returned per principal variation.",
}

JOB_START_OUTPUT = {
    "type": "object",
    "properties": {
        "job_id": {"type": "string"},
        "state": {"type": "string", "enum": ["running", "finished", "failed", "stopping", "stopped"]},
        "engine": {"type": "string"},
    },
    "required": ["job_id", "state"],
}

ANALYSIS_LINE = {
    "type": "object",
    "properties": {
        "multipv": {"type": "integer"},
        "depth": {"type": "integer"},
        "seldepth": {"type": "integer"},
        "score_cp": {"type": "integer", "description": "Evaluation in centipawns from the side to move."},
        "score_mate": {"type": "integer", "description": "Mate in N plies from the side to move (negative: getting mated)."},
        "bound": {"type": "string", "enum": ["lower", "upper"]},
        "nodes": {"type": "number"},
        "nps": {"type": "number"},
        "time_ms": {"type": "number"},
        "pv": {"type": "array", "items": {"type": "string"}, "description": "Principal variation in USI moves."},
        "pv_text": {"type": "string"},
    },
}

JOB_STATUS_COMMON = {
    "job_id": {"type": "string"},
    "kind": {"type": "string"},
    "state": {"type": "string", "enum": ["running", "finished", "failed", "stopping", "stopped"]},
    "elapsed_ms": {"type": "integer"},
    "error": {"type": "string"},
}

ANALYSIS_STATUS_OUTPUT = {
    "type": "object",
    "properties": {
        **JOB_STATUS_COMMON,
        "engine": {"type": "string"},
        "sfen": {"type": "string"},
        "depth": {"type": "integer"},
        "nodes": {"type": "number"},
        "nps": {"type": "number"},
        "lines": {"type": "array", "items": ANALYSIS_LINE},
        "bestmove": {"type": "string"},
        "ponder": {"type": "string"},
        "partial": {"type": "boolean", "description": "True when the job is still running and lines are provisional."},
    },
    "required": ["job_id", "state", "elapsed_ms", "lines"],
}

MATE_STATUS_OUTPUT = {
    "type": "object",
    "properties": {
        **JOB_STATUS_COMMON,
        "engine": {"type": "string"},
        "sfen": {"type": "string"},
        "status": {"type": "string", "enum": ["mate", "nomate", "unknown", "notimplemented"]},
        "pv": {"type": "array", "items": {"type": "string"}},
        "plies": {"type": "integer"},
    },
    "required": ["job_id", "state", "elapsed_ms"],
}

TSUME_STATUS_OUTPUT = {
    "type": "object",
    "properties": {
        **JOB_STATUS_COMMON,
        "engine": {"type": "string"},
        "target_moves": {"type": "integer"},
        "generated": {"type": "integer", "description": "Random candidates generated so far (including screened-out ones)."},
        "found": {"type": "integer"},
        "rejected": {"type": "integer", "description": "Candidates rejected by the uniqueness check."},
        "inconclusive": {"type": "integer"},
        "stopped": {"type": "boolean"},
        "positions": {
            "type": "array",
            "items": {
                "type": "object",
                "properties": {
                    "index": {"type": "integer"},
                    "sfen": {"type": "string"},
                    "pv": {"type": "array", "items": {"type": "string"}},
                },
                "required": ["sfen", "pv"],
            },
        },
    },
    "required": ["job_id", "state", "elapsed_ms", "found", "positions"],
}


# Output of tools whose operation the application performs after answering (AppClient.call_queued).
QUEUED_OUTPUT = {
    "interaction": {"type": "integer", "description": "Number of the queued operation in the application."},
    "state": {"type": "string", "enum": ["pending", "running", "done"],
              "description": "done: performed; running: it opened a modal dialog that is still open; "
                             "pending: not run yet within the wait."},
}
QUEUED_NOTE = (" The tool waits until the queued operation has run: a failure at run time (the target became "
               "hidden, disabled, blocked by a dialog or was closed) is returned as an error.")

# Generic GUI control tools: refused unless SHOGIBOARDQ_ALLOW_UNRESTRICTED_UI=1 (checked in server.dispatch_tool).
UNRESTRICTED_TOOLS: set[str] = set()


def _tool(
    name: str,
    title: str,
    description: str,
    input_schema: dict[str, Any],
    output_schema: dict[str, Any] | None = None,
    *,
    read_only: bool = False,
    destructive: bool = False,
    idempotent: bool = False,
    open_world: bool = False,
    unrestricted: bool = False,
) -> types.Tool:
    if unrestricted:
        UNRESTRICTED_TOOLS.add(name)
    schema = dict(input_schema)
    schema.setdefault("type", "object")
    schema.setdefault("additionalProperties", False)
    if output_schema and "queued" in output_schema.get("properties", {}):
        # The application answers before it performs a UI operation; AppClient.call_queued then waits for it.
        output_schema = {**output_schema, "properties": {**output_schema["properties"], **QUEUED_OUTPUT}}
        description += QUEUED_NOTE
    return types.Tool(
        name=name,
        title=title,
        description=description,
        inputSchema=schema,
        outputSchema=output_schema,
        annotations=types.ToolAnnotations(
            title=title,
            readOnlyHint=read_only,
            destructiveHint=destructive,
            idempotentHint=idempotent,
            openWorldHint=open_world,
        ),
    )


PHASE1_TOOLS: list[types.Tool] = [
    _tool(
        "convert_kifu",
        "Convert kifu",
        "Convert a shogi game record (kifu) to another format. Input is a file path or the record text in KIF, "
        "KI2, CSA, JKF, USI or USEN (auto-detected unless input_format is given). Output formats: kif, ki2, csa, "
        "jkf, usi, usen, or sfen (one SFEN per position of the main line). Returns the converted text (truncated "
        "to max_chars) plus the main-line USI moves, SFEN list and game information. Pass output_path to write "
        "the result to a file instead of returning the text. Does not require the GUI.",
        {**_spec("command", "convert-kifu",
                 override={"input_path": {"description": "Kifu file to read. " + ABS_PATH_DESC},
                           "output_path": {"description": "Write the converted record here (extension decides KIF/KI2 "
                                                          "Shift_JIS encoding). " + ABS_PATH_DESC}},
                 extra={"max_chars": {"type": "integer", "minimum": 1000, "maximum": 500_000, "default": 30000,
                                      "description": "Maximum characters of converted text to return."},
                        "max_moves": {"type": "integer", "minimum": 1, "maximum": 100_000, "default": 2000,
                                      "description": "Maximum main-line moves listed in usi_moves, moves and sfens "
                                                     "(ply_count is always the full count)."}}),
         "oneOf": [{"required": ["input_path"]}, {"required": ["text"]}]},
        {
            "type": "object",
            "properties": {
                "input_format": {"type": "string"},
                "output_format": {"type": "string"},
                "initial_sfen": {"type": "string"},
                "ply_count": {"type": "integer"},
                "usi_moves": {"type": "array", "items": {"type": "string"}},
                "sfens": {"type": "array", "items": {"type": "string"}},
                "moves": {"type": "array", "items": {"type": "object"}},
                "game_info": {"type": "array", "items": {"type": "object"}},
                "has_branches": {"type": "boolean"},
                "warnings": {"type": "array", "items": {"type": "string"}},
                "text": {"type": "string"},
                "truncated": {"type": "boolean"},
                "moves_truncated": {"type": "boolean"},
                "output_path": {"type": "string"},
            },
            "required": ["input_format", "output_format", "initial_sfen", "ply_count", "usi_moves"],
        },
        idempotent=True,
    ),
    _tool(
        "validate_sfen",
        "Validate SFEN",
        "Check whether a SFEN string is a legal shogi position. Returns the normalized SFEN, side to move, "
        "pieces in hand, whether the side to move is in check, the number of legal moves and the reasons if it "
        "is invalid. Tsume positions without the attacker's king are accepted.",
        _spec("command", "validate-sfen"),
        {
            "type": "object",
            "properties": {
                "sfen": {"type": "string"},
                "valid": {"type": "boolean"},
                "errors": {"type": "array", "items": {"type": "string"}},
                "normalized_sfen": {"type": "string"},
                "turn": {"type": "string", "enum": ["b", "w"]},
                "move_number": {"type": "integer"},
                "kings": {"type": "object"},
                "in_check": {"type": "boolean"},
                "opponent_in_check": {"type": "boolean"},
                "legal_move_count": {"type": "integer"},
                "hands": {"type": "object"},
            },
            "required": ["sfen", "valid", "errors"],
        },
        read_only=True,
        idempotent=True,
    ),
    _tool(
        "list_engines",
        "List engines",
        "List the USI engines registered in ShogiBoardQ (name, path, author, whether the executable exists). "
        "Use the returned names for the engine argument of other tools.",
        {"properties": {}},
        {
            "type": "object",
            "properties": {"engines": {"type": "array", "items": {"type": "object"}}},
            "required": ["engines"],
        },
        read_only=True,
        idempotent=True,
    ),
    _tool(
        "analyze_position",
        "Start position analysis",
        "Start analysing a position with a registered USI engine for a fixed number of seconds and return a "
        "job id immediately. Poll analysis_status for progress and analysis_result for the final evaluation, "
        "best move and principal variations (MultiPV lines).",
        _spec("command", "analyze", drop=["stdin_control"]),
        JOB_START_OUTPUT,
    ),
    _tool(
        "analysis_status",
        "Analysis status",
        "Return the state of an analysis job and the latest principal variations seen so far.",
        {"properties": {"job_id": JOB_ID, "max_pv_moves": MAX_PV_MOVES}, "required": ["job_id"]},
        ANALYSIS_STATUS_OUTPUT,
        read_only=True,
    ),
    _tool(
        "analysis_result",
        "Analysis result",
        "Return the final result of an analysis job: best move, ponder move and the last evaluation of each "
        "MultiPV line. While the job is still running the current lines are returned with partial=true.",
        {"properties": {"job_id": JOB_ID, "max_pv_moves": MAX_PV_MOVES}, "required": ["job_id"]},
        ANALYSIS_STATUS_OUTPUT,
        read_only=True,
    ),
    _tool(
        "search_mate",
        "Start mate search",
        "Start a mate (tsume) search with a registered engine that supports 'go mate' and return a job id. "
        "Poll mate_status for the result: mate with the mating sequence, nomate, unknown (time ran out) or "
        "notimplemented (the engine cannot search for mate).",
        _spec("command", "mate", drop=["stdin_control"]),
        JOB_START_OUTPUT,
    ),
    _tool(
        "mate_status",
        "Mate search status",
        "Return the state and, when finished, the outcome and mating sequence of a mate search job.",
        {"properties": {"job_id": JOB_ID}, "required": ["job_id"]},
        MATE_STATUS_OUTPUT,
        read_only=True,
    ),
    _tool(
        "generate_tsume",
        "Start tsume generation",
        "Start generating tsume shogi (mate) problems: random candidate positions are screened by the built-in "
        "solver, checked with the engine's 'go mate', verified to have a unique solution and trimmed of "
        "unnecessary pieces. Returns a job id; poll tsume_generation_status for found positions and call "
        "stop_tsume_generation to end early. Requires an engine that supports 'go mate' (e.g. KomoringHeights).",
        _spec("command", "generate-tsume", drop=["stdin_control"]),
        JOB_START_OUTPUT,
    ),
    _tool(
        "tsume_generation_status",
        "Tsume generation status",
        "Return progress counters and the problems found so far by a tsume generation job.",
        {
            "properties": {
                "job_id": JOB_ID,
                "max_positions": {"type": "integer", "minimum": 1, "maximum": 100, "default": 20, "description": "Maximum number of positions to include."},
            },
            "required": ["job_id"],
        },
        TSUME_STATUS_OUTPUT,
        read_only=True,
    ),
    _tool(
        "stop_tsume_generation",
        "Stop tsume generation",
        "Stop a running tsume generation job and return its final counters and positions.",
        {"properties": {"job_id": JOB_ID}, "required": ["job_id"]},
        TSUME_STATUS_OUTPUT,
        idempotent=True,
    ),
    _tool(
        "verify_tsume",
        "Verify tsume problem",
        "Check a tsume shogi problem for a unique solution of the given length using a registered engine that "
        "supports 'go mate'. Returns unique (main line included), multiple (a cook exists), nomate, wrong_length, "
        "unknown (time budget exhausted) or invalid. Runs synchronously; keep timeout_seconds small.",
        # The call waits for the result, so the budget is shorter than the CLI allows
        _spec("command", "verify-tsume", override={"timeout_seconds": {"maximum": 50}}),
        {
            "type": "object",
            "properties": {
                "status": {"type": "string", "enum": ["unique", "multiple", "nomate", "wrong_length", "unknown", "invalid"]},
                "pv": {"type": "array", "items": {"type": "string"}},
                "queries": {"type": "integer"},
                "elapsed_ms": {"type": "number"},
                "sfen": {"type": "string"},
                "target_moves": {"type": "integer"},
            },
            "required": ["status", "pv", "sfen", "target_moves"],
        },
        read_only=True,
    ),
    _tool(
        "render_board_image",
        "Render board image",
        "Render a position as a PNG image (same piece images and colours as the ShogiBoardQ GUI) and return the "
        "file path. Optionally highlight the last move and show player names. Labels such as the turn badge and the "
        "rank coordinates follow the interface language and move notation set in ShogiBoardQ.",
        _spec("command", "render-board", override={"output_path": {"description": "Destination .png path. " + ABS_PATH_DESC}}),
        {
            "type": "object",
            "properties": {"output_path": {"type": "string"}, "width": {"type": "integer"}, "height": {"type": "integer"}},
            "required": ["output_path", "width", "height"],
        },
        idempotent=True,
    ),
    _tool(
        "list_jobs",
        "List jobs",
        "List analysis, mate search and tsume generation jobs of this server session with their states.",
        {"properties": {}},
        {"type": "object", "properties": {"jobs": {"type": "array", "items": {"type": "object"}}}, "required": ["jobs"]},
        read_only=True,
    ),
    _tool(
        "cancel_job",
        "Cancel job",
        "Stop a running job of any kind (analysis, mate search, tsume generation). Finished jobs are unaffected.",
        {"properties": {"job_id": JOB_ID}, "required": ["job_id"]},
        {"type": "object", "properties": {"job_id": {"type": "string"}, "state": {"type": "string"}}, "required": ["job_id", "state"]},
        idempotent=True,
    ),
]

PHASE2_TOOLS: list[types.Tool] = [
    _tool(
        "get_app_state",
        "Get app state",
        "Return the state of the running ShogiBoardQ window: UI state (idle, game, analysis, CSA game, tsume "
        "search, consideration, position edit), play mode, current ply and total plies, current kifu file, "
        "unsaved changes, open dialogs and engine names. During CSA sessions csa reports connection/game "
        "state, side, player type and remaining/consumed milliseconds at the last server-confirmed turn. "
        "Starts ShogiBoardQ with --automation if "
        "SHOGIBOARDQ_EXECUTABLE is set and no instance is running.",
        {"properties": {}},
        {"type": "object", "properties": {"ui_state": {"type": "string"}, "play_mode": {"type": "string"},
                                          "current_ply": {"type": "integer"}, "total_plies": {"type": "integer"},
                                          "kifu_file": {"type": "string"}, "dirty": {"type": "boolean"},
                                          "dialogs": {"type": "array"}, "csa": {"type": "object"}},
         "required": ["ui_state"]},
        read_only=True,
    ),
    _tool(
        "get_position",
        "Get current position",
        "Return the position shown in ShogiBoardQ as SFEN, together with the start position and the USI moves "
        "from the start position to the current ply.",
        {"properties": {}},
        {"type": "object", "properties": {"sfen": {"type": "string"}, "start_sfen": {"type": "string"},
                                          "ply": {"type": "integer"}, "moves": {"type": "array", "items": {"type": "string"}}},
         "required": ["sfen"]},
        read_only=True,
    ),
    _tool(
        "set_position",
        "Set position",
        "Replace the position shown in ShogiBoardQ with the given SFEN (the record is cleared). Fails with "
        "unsaved_changes when the current record has unsaved edits unless discard_unsaved is true.",
        _spec("method", "position.set"),
        {"type": "object", "properties": {"sfen": {"type": "string"}}, "required": ["sfen"]},
        destructive=True,
    ),
    _tool(
        "load_kifu",
        "Load kifu",
        "Load a game record into ShogiBoardQ from a file (KIF/KI2/CSA/JKF/USI/USEN/SFEN) or from text. Fails "
        "with unsaved_changes when the current record has unsaved edits unless discard_unsaved is true.",
        {**_spec("method", "kifu.load", override={"path": {"description": "Kifu file. " + ABS_PATH_DESC}}),
         "oneOf": [{"required": ["path"]}, {"required": ["text"]}]},
        {"type": "object", "properties": {"total_plies": {"type": "integer"}, "start_sfen": {"type": "string"},
                                          "kifu_file": {"type": "string"}}, "required": ["total_plies"]},
        destructive=True,
    ),
    _tool(
        "save_kifu",
        "Save kifu",
        "Save the record shown in ShogiBoardQ to a file. The format follows the extension (.kif .kifu .ki2 .csa "
        ".ki2u .jkf .usen .usi). Existing files are only replaced with overwrite=true.",
        _spec("method", "kifu.save", override={"path": {"description": "Destination file. " + ABS_PATH_DESC}}),
        {"type": "object", "properties": {"path": {"type": "string"}, "format": {"type": "string"}}, "required": ["path"]},
    ),
    _tool(
        "get_kifu",
        "Get kifu",
        "Return the record shown in ShogiBoardQ. format=moves (default) gives a structured list of the line "
        "being displayed (the variation when one is shown; line=main for the main line) with ply, Japanese "
        "notation, USI move, time and comment, limited by from_ply/max_moves; the other formats return the "
        "exported text (with all variations) truncated to max_chars.",
        _spec("method", "kifu.get", extra={"max_chars": {"type": "integer", "minimum": 1000, "maximum": 500_000,
                                                        "default": 30000,
                                                        "description": "Maximum characters of exported text to return."}}),
        {"type": "object", "properties": {"format": {"type": "string"}, "total_plies": {"type": "integer"},
                                          "moves": {"type": "array", "items": {"type": "object"}},
                                          "text": {"type": "string"}, "truncated": {"type": "boolean"}},
         "required": ["format", "total_plies"]},
        read_only=True,
    ),
    _tool(
        "goto_ply",
        "Go to ply",
        "Navigate the record shown in ShogiBoardQ to the given ply (0 = start position) on the current line.",
        _spec("method", "kifu.goto"),
        {"type": "object", "properties": {"ply": {"type": "integer"}, "sfen": {"type": "string"}}, "required": ["ply"]},
        idempotent=True,
    ),
    _tool(
        "list_actions",
        "List menu actions",
        "List the menu actions (QAction object names) that trigger_action may run, with their labels and "
        "whether they are currently enabled or checked.",
        {"properties": {}},
        {"type": "object", "properties": {"actions": {"type": "array", "items": {"type": "object"}}}, "required": ["actions"]},
        read_only=True,
    ),
    _tool(
        "trigger_action",
        "Trigger menu action",
        "Trigger a ShogiBoardQ menu action by its QAction object name (see list_actions), e.g. "
        "actionAnalyzeKifu, actionTsumeShogiSearch, actionTsumeshogiGenerator, actionSfenCollectionViewer, "
        "actionCopySFEN, actionFlipBoard. Only allow-listed actions run; quitting, overwriting the current file "
        "are refused. Language changes take effect after restarting the app. Actions that open dialogs return immediately; inspect them with "
        "list_dialogs and get_widget_text.",
        _spec("method", "action.trigger"),
        {"type": "object", "properties": {"name": {"type": "string"}, "triggered": {"type": "boolean"}}, "required": ["name", "triggered"]},
        unrestricted=True,
    ),
    _tool(
        "click_board_square",
        "Click board square",
        "Click a square on the visible board through its normal mouse event handler. Click the source, then "
        "the destination to make a move. Coordinates are logical and do not change when the board is flipped. "
        "For hand pieces use file=10 (Black), ranks 1..8=P,L,N,S,G,B,R,K; file=11 (White), ranks "
        "9..2=P,L,N,S,G,B,R,K. Right-click cancels selection. Returns when the click is queued, not when "
        "a move is accepted or the engine finishes. Use get_widget_text/capture_screenshot to inspect the "
        "result. A promotion dialog can be answered with click_dialog_button. Hidden, disabled or modally "
        "blocked boards are refused; normal game input rules still apply.",
        _spec("method", "board.click"),
        {"type": "object", "properties": {"target": {"type": "string"}, "file": {"type": "integer"},
            "rank": {"type": "integer"}, "queued": {"type": "boolean"}}, "required": ["target", "file", "rank", "queued"]},
    ),
    _tool(
        "click_dialog_button",
        "Click dialog button",
        "Click a visible, enabled button in an open dialog. Select by widget object name/selector OR exact text "
        "(e.g. 成る in a promotion dialog). index selects the zero-based occurrence when several buttons "
        "match, such as tsumeProblemCard in the problem collection. The click is queued so that modal "
        "dialogs return immediately. A dialog blocked by another modal window cannot be clicked.",
        {**_spec("method", "dialog.clickButton"), "oneOf": [{"required": ["widget"]}, {"required": ["text"]}]},
        {"type": "object", "properties": {"queued": {"type": "boolean"}, "object_name": {"type": "string"},
            "text": {"type": "string"}}, "required": ["queued", "object_name", "text"]},
        unrestricted=True,
    ),
    _tool(
        "list_docks", "List dock panels",
        "Inspect all main-window docks, including closed panels. Returns object_name, title, floating, "
        "area (last dock area for floating panels), tabified_with, allowed_areas, closable/movable/floatable, "
        "toggle_checked and global client geometry. hidden means explicitly closed; visible can be true "
        "for an inactive tab. exposed reports whether the dock has a visible region.",
        {"properties": {}},
        {"type": "object", "properties": {"docks": {"type": "array", "items": {"type": "object"}}},
         "required": ["docks"]},
        read_only=True,
    ),
    _tool(
        "configure_dock", "Configure dock panel",
        "Show/raise, close (hide), float, dock to an area, or tabify a named panel using Qt dock APIs. "
        "The drag operation sends mouse events to a visible docked panel's Qt title bar, ending at global "
        "coordinates x/y; it cannot drag OS-native floating title bars. "
        "Movement respects dock locks and allowed areas; a tabify relative must be visible and docked. "
        "For float, optional geometry is the global client rectangle (Qt minimum sizes still apply). "
        "Operations are queued and blocked by modal dialogs; poll list_docks to check completion. "
        "Use trigger_action(actionResetDockLayout) to reset the layout.",
        _spec("method", "dock.configure"),
        {"type": "object", "properties": {"queued": {"type": "boolean"}, "object_name": {"type": "string"}},
         "required": ["queued", "object_name"]},
    ),
    _tool(
        "show_dock", "Show dock panel",
        "Show and raise a dock panel, including panels tabbed behind another dock. Select by object "
        "name (e.g. ConsiderationDock or ThinkingDock); get_widget_text lists dock names. "
        "The operation is queued and refused when the main window is modally blocked.",
        _spec("method", "widget.showDock"),
        {"type": "object", "properties": {"queued": {"type": "boolean"}, "object_name": {"type": "string"}},
         "required": ["queued", "object_name"]},
    ),
    _tool(
        "click_widget", "Click widget button",
        "Click a button by object name or selector from get_widget_text in the main window or a dialog, including considerationStartStop, "
        "considerationFontIncrease and considerationEngineSettings. Uses normal button signals. "
        "The click is queued; inspect the result with get_widget_text. Hidden, disabled and modally "
        "blocked widgets are refused.",
        _spec("method", "widget.click"),
        {"type": "object", "properties": {"queued": {"type": "boolean"}, "object_name": {"type": "string"}},
         "required": ["queued", "object_name"]},
        unrestricted=True,
    ),
    _tool(
        "get_clipboard", "Read clipboard",
        "Read the running application's clipboard text and image dimensions. Use after copy menu actions "
        "to verify their actual output. Text is limited by max_chars; truncated indicates omitted text. "
        "The clipboard is shared with every other application, so this is a generic GUI tool.",
        _spec("method", "clipboard.get"),
        {"type": "object", "properties": {"text": {"type": "string"}, "truncated": {"type": "boolean"},
                                          "has_image": {"type": "boolean"}, "image_width": {"type": "integer"},
                                          "image_height": {"type": "integer"}},
         "required": ["text", "truncated", "has_image", "image_width", "image_height"]},
        read_only=True,
        unrestricted=True,
    ),
    _tool(
        "edit_table_cell", "Edit table cell",
        "Edit a visible, editable table cell through its standard line editor. Row and column are zero-based. "
        "By default Enter commits the edit; commit=false leaves the editor open. Read-only or modally blocked cells are refused.",
        _spec("method", "widget.editCell"),
        {"type": "object", "properties": {"queued": {"type": "boolean"}, "object_name": {"type": "string"}},
         "required": ["queued", "object_name"]},
        unrestricted=True,
    ),
    _tool(
        "set_widget_value", "Set widget value",
        "Change a widget by object name or selector from get_widget_text: combo box (item text or index), "
        "spin box/slider (number), check box/radio/checkable group (boolean), list/tab (index), "
        "color dialog (color string), or editable text field (plain text). For example: "
        "considerationEngine, considerationMultiPV, considerationSeconds, considerationUnlimited, "
        "considerationArrows. Normal change signals run; the operation is queued so dialogs can open. "
        "Use submit=true on a line edit to press Enter after entering text (for example csaCommandInput). "
        "Requires SHOGIBOARDQ_ALLOW_UNRESTRICTED_UI=1. Hidden, disabled, read-only or modally blocked widgets and out-of-range values are refused. Password fields accept write-only input; revealing them is forbidden.",
        _spec("method", "widget.setValue"),
        {"type": "object", "properties": {"queued": {"type": "boolean"}, "object_name": {"type": "string"}},
         "required": ["queued", "object_name"]},
        unrestricted=True,
    ),
    _tool(
        "click_table_cell", "Click table cell",
        "Click a zero-based row and column in a table identified by object name or selector through normal mouse events. "
        "Use considerationView column 5 (header 盤面) to open the principal variation board. Scrolls the cell "
        "into view; hidden, disabled and modally blocked cells are refused. Returns when queued.",
        _spec("method", "widget.clickCell"),
        {"type": "object", "properties": {"queued": {"type": "boolean"}, "object_name": {"type": "string"}},
         "required": ["queued", "object_name"]},
        unrestricted=True,
    ),
    _tool(
        "capture_screenshot",
        "Capture screenshot",
        "Save a PNG screenshot of the ShogiBoardQ main window (target='main') or of an open dialog identified "
        "by its object name, selector or window title, and return the file path.",
        _spec("method", "screenshot.capture",
              override={"output_dir": {"description": "Directory for the PNG. Defaults to SHOGIBOARDQ_OUTPUT_DIR or a temp "
                                                      "directory. " + ABS_PATH_DESC}}),
        {"type": "object", "properties": {"path": {"type": "string"}, "width": {"type": "integer"}, "height": {"type": "integer"}},
         "required": ["path"]},
        idempotent=True,
    ),
    _tool(
        "list_dialogs",
        "List dialogs",
        "List the top-level windows and dialogs currently open in ShogiBoardQ (object name, selector, class, title, "
        "visibility, modality).",
        {"properties": {}},
        {"type": "object", "properties": {"windows": {"type": "array", "items": {"type": "object"}}}, "required": ["windows"]},
        read_only=True,
    ),
    _tool(
        "close_dialog",
        "Close dialog",
        "Close an open dialog (reject it) identified by its object name, selector or window title substring. Use it to "
        "dismiss dialogs opened by trigger_action or error message boxes.",
        _spec("method", "dialog.close"),
        {"type": "object", "properties": {"closed": {"type": "boolean"}, "object_name": {"type": "string"}, "title": {"type": "string"}},
         "required": ["closed"]},
        idempotent=True,
    ),
    _tool(
        "get_widget_text",
        "Get widget text",
        "Read labels, text fields, combo boxes, check boxes, lists, tables and boards inside an open dialog (or the main "
        "window when target is omitted). Restrict to one widget by object name or selector. Intended for verifying "
        "what the GUI shows. Widgets include geometry relative to the target window; boards include board_sfen "
        "(board pieces only), flipped, square_size and candidate arrows. Combo boxes and tabs include current_index.",
        _spec("method", "widget.text"),
        {"type": "object", "properties": {"widgets": {"type": "array", "items": {"type": "object"}}}, "required": ["widgets"]},
        read_only=True,
    ),
]

PHASE2_TOOLS += [
    _tool(
        "list_menu_actions", "List menu items",
        "Read a menu (or a menu button) by object name or widget selector. Returns nested items with zero-based "
        "indices for select_menu_action. Includes dynamic saved layouts, joseki merge/history and collection history.",
        _spec("method", "menu.items"),
        {"type": "object", "properties": {"items": {"type": "array", "items": {"type": "object"}}}, "required": ["items"]},
        read_only=True,
    ),
    _tool(
        "select_menu_action", "Select menu item",
        "Trigger an allowed menu item using a path of indices from list_menu_actions. "
        "For example [0,0] selects the first action of the first saved layout. Disabled or blocked actions are refused.",
        _spec("method", "menu.select"),
        {"type": "object", "properties": {"queued": {"type": "boolean"}, "text": {"type": "string"}}, "required": ["queued", "text"]},
        unrestricted=True,
    ),
    _tool(
        "menu_favorites", "Get or replace menu favorites",
        "Read favorites and available action names. With actions, replace the ordered list (add/remove/reorder) "
        "and persist it. Empty actions clears favorites. Unknown names and duplicates are rejected.",
        _spec("method", "menu.favorites",
              override={"actions": {"items": {"type": "string", "minLength": 1}, "uniqueItems": True}}),
        {"type": "object", "properties": {"actions": {"type": "array", "items": {"type": "string"}},
                                          "available_actions": {"type": "array", "items": {"type": "string"}}},
         "required": ["actions", "available_actions"]},
    ),
    _tool(
        "click_branch_node", "Select branch tree node",
        "Click a branch tree node identified by id from get_widget_text(widget='branchTreeView'). "
        "Scrolls to the node and follows the normal GUI navigation path. Refresh node ids after reloading a record.",
        _spec("method", "branch.click"),
        {"type": "object", "properties": {"queued": {"type": "boolean"}, "id": {"type": "integer"}}, "required": ["queued", "id"]},
    ),
]

KIFU_ANALYSIS_OUTPUT = {
    "type": "object",
    "properties": {**JOB_STATUS_COMMON, "completed": {"type": "integer"},
                   "total_positions": {"type": "integer"}, "positions": {"type": "array", "items": {"type": "object"}},
                   "partial": {"type": "boolean"}, "truncated": {"type": "boolean"},
                   "offset": {"type": "integer"}, "next_offset": {"type": "integer"}},
    "required": ["job_id", "state", "completed", "positions", "partial", "truncated", "next_offset"],
}

PHASE1_TOOLS += [
    _tool(
        "analyze_kifu", "Analyze whole game record",
        "Analyze every main-line position in a record as one cancellable job without changing the GUI. "
        "Accepts a file or text. The range is inclusive: ply 0 is the initial position, ply N is after N moves. "
        "Each position retains the move history for repetition. Poll kifu_analysis_status/result; cancel_job stops it. "
        "Variations are not analyzed; has_branches and warnings are returned.",
        {**_spec("command", "analyze-kifu", drop=["stdin_control"],
                 override={"input_path": {"description": "Kifu file to read. " + ABS_PATH_DESC}}),
         "oneOf": [{"required": ["input_path"], "not": {"required": ["text"]}},
                   {"required": ["text"], "not": {"required": ["input_path"]}}]},
        JOB_START_OUTPUT,
    ),
    *[_tool(
        name, title,
        "Get whole-record analysis progress and a page of results. positions includes ply, SFEN, played_move, "
        "bestmove and MultiPV lines with side-to-move scores, plus score_cp_black/score_mate_black for Black's perspective. "
        "Results completed before cancellation or failure remain available. Use next_offset to read more results.",
        {"properties": {"job_id": JOB_ID, "offset": {"type": "integer", "minimum": 0, "default": 0},
                        "max_positions": {"type": "integer", "minimum": 1, "maximum": 1000, "default": 100},
                        "max_pv_moves": MAX_PV_MOVES}, "required": ["job_id"]},
        KIFU_ANALYSIS_OUTPUT, read_only=True,
    ) for name, title in (("kifu_analysis_status", "Get whole-record analysis progress"),
                          ("kifu_analysis_result", "Get whole-record analysis results"))],
]

ALL_TOOLS: list[types.Tool] = PHASE1_TOOLS + PHASE2_TOOLS

RESOURCES: list[types.Resource] = [
    types.Resource(
        uri="shogiboardq://position/current",  # type: ignore[arg-type]
        name="Current position",
        title="Current position (SFEN)",
        description="SFEN of the position shown in the running ShogiBoardQ window.",
        mimeType="text/plain",
    ),
    types.Resource(
        uri="shogiboardq://kifu/current",  # type: ignore[arg-type]
        name="Current kifu",
        title="Current game record (KIF)",
        description="The record shown in the running ShogiBoardQ window, exported as KIF text.",
        mimeType="text/plain",
    ),
    types.Resource(
        uri="shogiboardq://engines",  # type: ignore[arg-type]
        name="Registered engines",
        title="Registered USI engines",
        description="USI engines registered in ShogiBoardQ as JSON.",
        mimeType="application/json",
    ),
]
