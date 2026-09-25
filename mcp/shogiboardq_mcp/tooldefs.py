"""Tool definitions: names, English descriptions, strict JSON Schemas, annotations.

Phase 1 tools run through ``shogiboardq-cli`` and need no GUI. Phase 2 tools
talk to a running ``ShogiBoardQ --automation`` instance.
"""

from __future__ import annotations

from typing import Any

import mcp.types as types

USI_MOVE_PATTERN = r"^(?:[1-9][a-i][1-9][a-i]\+?|[PLNSGBR]\*[1-9][a-i])$"

ENGINE = {
    "type": "string",
    "minLength": 1,
    "description": "Name of a USI engine registered in ShogiBoardQ, exactly as returned by list_engines.",
}
SFEN = {
    "type": "string",
    "minLength": 1,
    "description": "Position in SFEN (board, side to move b/w, hands, move number), or 'startpos' for the initial "
    "position. A leading 'position sfen ' is accepted.",
}
MOVES = {
    "type": "array",
    "items": {"type": "string", "pattern": USI_MOVE_PATTERN},
    "default": [],
    "maxItems": 1000,
    "description": "USI moves played after the start position, e.g. [\"7g7f\", \"3c3d\"]. Drops use 'P*5e'.",
}
JOB_ID = {"type": "string", "minLength": 1, "description": "Job id returned by the tool that started the job."}
ABS_PATH_DESC = "Absolute path inside an allowed directory (default: the user's home directory)."
OVERWRITE = {
    "type": "boolean",
    "default": False,
    "description": "Allow replacing an existing file. Without it an existing file is an error.",
}
MAX_PV_MOVES = {
    "type": "integer",
    "minimum": 1,
    "maximum": 200,
    "default": 12,
    "description": "Maximum number of moves returned per principal variation.",
}
KIFU_FORMATS = ["kif", "ki2", "csa", "jkf", "usi", "usen"]

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
) -> types.Tool:
    schema = dict(input_schema)
    schema.setdefault("type", "object")
    schema.setdefault("additionalProperties", False)
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
        {
            "properties": {
                "input_path": {"type": "string", "description": "Kifu file to read. " + ABS_PATH_DESC},
                "text": {"type": "string", "minLength": 1, "maxLength": 2_000_000, "description": "Kifu text instead of a file."},
                "input_format": {"type": "string", "enum": ["auto", *KIFU_FORMATS], "default": "auto"},
                "output_format": {"type": "string", "enum": [*KIFU_FORMATS, "sfen"]},
                "output_path": {"type": "string", "description": "Write the converted record here (extension decides KIF/KI2 Shift_JIS encoding). " + ABS_PATH_DESC},
                "overwrite": OVERWRITE,
                "max_chars": {"type": "integer", "minimum": 1000, "maximum": 500_000, "default": 30000,
                              "description": "Maximum characters of converted text to return."},
            },
            "required": ["output_format"],
            "oneOf": [{"required": ["input_path"]}, {"required": ["text"]}],
        },
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
        {"properties": {"sfen": SFEN}, "required": ["sfen"]},
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
        {
            "properties": {
                "engine": ENGINE,
                "sfen": {**SFEN, "default": "startpos"},
                "moves": MOVES,
                "seconds": {"type": "integer", "minimum": 1, "maximum": 600, "default": 10, "description": "Thinking time in seconds."},
                "multipv": {"type": "integer", "minimum": 1, "maximum": 10, "default": 1, "description": "Number of candidate lines."},
            },
            "required": ["engine"],
        },
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
        {
            "properties": {
                "engine": ENGINE,
                "sfen": SFEN,
                "moves": MOVES,
                "seconds": {"type": "integer", "minimum": 1, "maximum": 600, "default": 10},
            },
            "required": ["engine", "sfen"],
        },
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
        {
            "properties": {
                "engine": ENGINE,
                "target_moves": {"type": "integer", "minimum": 1, "maximum": 19, "default": 3, "description": "Mate length in plies (odd)."},
                "max_positions": {"type": "integer", "minimum": 1, "maximum": 100, "default": 5, "description": "Stop after this many problems."},
                "timeout_ms": {"type": "integer", "minimum": 500, "maximum": 60000, "default": 5000, "description": "Engine time per candidate, per trimming step and per verification."},
                "max_attack_pieces": {"type": "integer", "minimum": 1, "maximum": 10, "default": 4},
                "max_defend_pieces": {"type": "integer", "minimum": 0, "maximum": 10, "default": 1, "description": "Defending pieces other than the king."},
                "attack_range": {"type": "integer", "minimum": 1, "maximum": 8, "default": 3, "description": "Placement range around the king in squares."},
                "add_remaining_to_defender_hand": {"type": "boolean", "default": True, "description": "Give unused pieces to the defender's hand (tsume convention)."},
                "allow_final_move_alternatives": {"type": "boolean", "default": True, "description": "Accept problems whose final mating move has alternatives."},
            },
            "required": ["engine"],
        },
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
        {
            "properties": {
                "engine": ENGINE,
                "sfen": SFEN,
                "target_moves": {"type": "integer", "minimum": 1, "maximum": 39, "description": "Expected mate length in plies (odd)."},
                "timeout_seconds": {"type": "integer", "minimum": 1, "maximum": 50, "default": 15},
                "allow_final_move_alternatives": {"type": "boolean", "default": True},
            },
            "required": ["engine", "sfen", "target_moves"],
        },
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
        "file path. Optionally highlight the last move and show player names.",
        {
            "properties": {
                "sfen": {**SFEN, "default": "startpos"},
                "output_path": {"type": "string", "description": "Destination .png path. " + ABS_PATH_DESC},
                "square_size": {"type": "integer", "minimum": 20, "maximum": 150, "default": 50, "description": "Square width in pixels."},
                "flip": {"type": "boolean", "default": False, "description": "View from white's (gote's) side."},
                "last_move": {"type": "string", "pattern": USI_MOVE_PATTERN, "description": "USI move to highlight."},
                "black_name": {"type": "string", "maxLength": 64},
                "white_name": {"type": "string", "maxLength": 64},
                "overwrite": OVERWRITE,
            },
            "required": ["output_path"],
        },
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

PHASE2_TOOLS: list[types.Tool] = []

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
