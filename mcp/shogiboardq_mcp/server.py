"""MCP server wiring: tools, resources and the stdio transport."""

from __future__ import annotations

import json
import logging
from collections.abc import Awaitable, Callable
from typing import Any

import mcp.types as types
from mcp.server.lowlevel import Server
from mcp.server.lowlevel.helper_types import ReadResourceContents
from mcp.server.stdio import stdio_server

from . import __version__
from .app_client import AppClient
from .cli_backend import run_cli
from .errors import ToolError
from .handlers_app import AppTools
from .handlers_cli import CliTools
from .jobs import JobManager
from .tooldefs import ALL_TOOLS, RESOURCES

log = logging.getLogger(__name__)

Handler = Callable[[dict[str, Any]], Awaitable[tuple[str, dict[str, Any]]]]

INSTRUCTIONS = (
    "ShogiBoardQ tools. Positions are SFEN strings ('startpos' = initial position) and moves are USI "
    "(e.g. 7g7f, P*5e). Engines are referenced by the name shown by list_engines. Long operations "
    "(analyze_position, search_mate, generate_tsume) return a job_id; poll the matching *_status tool. "
    "File paths must be absolute and inside the allowed directories. Tools whose name refers to the app "
    "(get_app_state, load_kifu, trigger_action, capture_screenshot, ...) need a running ShogiBoardQ started "
    "with --automation; the server launches one when SHOGIBOARDQ_EXECUTABLE is set."
)


def build_server() -> tuple[Server, JobManager, AppClient]:
    jobs = JobManager()
    client = AppClient()
    cli_tools = CliTools(jobs)
    app_tools = AppTools(client)

    handlers: dict[str, Handler] = {
        "convert_kifu": cli_tools.convert_kifu,
        "validate_sfen": cli_tools.validate_sfen,
        "list_engines": cli_tools.list_engines,
        "analyze_position": cli_tools.analyze_position,
        "analysis_status": cli_tools.analysis_status,
        "analysis_result": cli_tools.analysis_result,
        "search_mate": cli_tools.search_mate,
        "mate_status": cli_tools.mate_status,
        "generate_tsume": cli_tools.generate_tsume,
        "tsume_generation_status": cli_tools.tsume_generation_status,
        "stop_tsume_generation": cli_tools.stop_tsume_generation,
        "verify_tsume": cli_tools.verify_tsume,
        "render_board_image": cli_tools.render_board_image,
        "list_jobs": cli_tools.list_jobs,
        "cancel_job": cli_tools.cancel_job,
        "get_app_state": app_tools.get_app_state,
        "get_position": app_tools.get_position,
        "set_position": app_tools.set_position,
        "load_kifu": app_tools.load_kifu,
        "save_kifu": app_tools.save_kifu,
        "get_kifu": app_tools.get_kifu,
        "goto_ply": app_tools.goto_ply,
        "list_actions": app_tools.list_actions,
        "trigger_action": app_tools.trigger_action,
        "capture_screenshot": app_tools.capture_screenshot,
        "list_dialogs": app_tools.list_dialogs,
        "close_dialog": app_tools.close_dialog,
        "get_widget_text": app_tools.get_widget_text,
    }
    missing = {t.name for t in ALL_TOOLS} ^ set(handlers)
    assert not missing, f"tool/handler mismatch: {missing}"

    server: Server = Server("shogiboardq", version=__version__, instructions=INSTRUCTIONS)

    @server.list_tools()
    async def list_tools() -> list[types.Tool]:
        return ALL_TOOLS

    @server.call_tool()
    async def call_tool(name: str, arguments: dict[str, Any]):
        handler = handlers.get(name)
        if handler is None:
            return _error_result("unknown_tool", f"Unknown tool {name!r}")
        try:
            text, structured = await handler(arguments or {})
        except ToolError as exc:
            log.info("tool %s failed: %s", name, exc)
            return _error_result(exc.code, exc.message, exc.data)
        except Exception as exc:  # pragma: no cover - defensive
            log.exception("tool %s crashed", name)
            return _error_result("internal_error", f"{type(exc).__name__}: {exc}")
        return [types.TextContent(type="text", text=text)], structured

    @server.list_resources()
    async def list_resources() -> list[types.Resource]:
        return RESOURCES

    @server.read_resource()
    async def read_resource(uri) -> list[ReadResourceContents]:
        key = str(uri)
        try:
            if key == "shogiboardq://position/current":
                pos = await client.call("position.get")
                return [ReadResourceContents(content=str(pos.get("sfen", "")), mime_type="text/plain")]
            if key == "shogiboardq://kifu/current":
                result = await client.call("kifu.get", {"format": "kif", "max_moves": 2000})
                return [ReadResourceContents(content=str(result.get("text", "")), mime_type="text/plain")]
            if key == "shogiboardq://engines":
                result = await run_cli(["list-engines"])
                return [ReadResourceContents(content=json.dumps(result.get("engines", []), ensure_ascii=False, indent=2),
                                             mime_type="application/json")]
        except ToolError as exc:
            raise ValueError(f"{exc.code}: {exc.message}") from exc
        raise ValueError(f"Unknown resource: {key}")

    return server, jobs, client


def _error_result(code: str, message: str, data: dict | None = None) -> types.CallToolResult:
    text = f"Error ({code}): {message}"
    if data and data.get("hint"):
        text += f"\nHint: {data['hint']}"
    return types.CallToolResult(content=[types.TextContent(type="text", text=text)], isError=True)


async def run() -> None:
    server, jobs, client = build_server()
    try:
        async with stdio_server() as (read_stream, write_stream):
            await server.run(read_stream, write_stream, server.create_initialization_options())
    finally:
        await jobs.shutdown()
        await client.shutdown()
