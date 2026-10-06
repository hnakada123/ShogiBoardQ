"""MCP server wiring: tools, resources and the stdio transport."""

from __future__ import annotations

import json
import logging
from collections.abc import Awaitable, Callable
from typing import Any

import mcp.types as types
from mcp.server.lowlevel import Server
from mcp.server.stdio import stdio_server

from . import __version__
from .app_client import AppClient
from .cli_backend import run_cli
from .errors import ToolError
from .handlers_app import AppTools
from .handlers_cli import CliTools
from .handlers_kifu_analysis import KifuAnalysisTools
from .jobs import JobManager
from .tooldefs import ALL_TOOLS, RESOURCES

log = logging.getLogger(__name__)

Handler = Callable[[dict[str, Any]], Awaitable[tuple[str, dict[str, Any]]]]
ToolOutcome = tuple[str, dict[str, Any]] | types.CallToolResult
ToolDispatcher = Callable[[str, dict[str, Any] | None], Awaitable[ToolOutcome]]
ResourceReader = Callable[[str], Awaitable[tuple[str, str]]]

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
    kifu_analysis = KifuAnalysisTools(jobs, cli_tools)
    app_tools = AppTools(client)

    handlers: dict[str, Handler] = {
        "convert_kifu": cli_tools.convert_kifu,
        "validate_sfen": cli_tools.validate_sfen,
        "list_engines": cli_tools.list_engines,
        "analyze_position": cli_tools.analyze_position,
        "analysis_status": cli_tools.analysis_status,
        "analysis_result": cli_tools.analysis_result,
        "analyze_kifu": kifu_analysis.analyze_kifu,
        "kifu_analysis_status": kifu_analysis.status,
        "kifu_analysis_result": kifu_analysis.status,
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
        "get_clipboard": app_tools.get_clipboard,
        "edit_table_cell": app_tools.edit_table_cell,
        "set_position": app_tools.set_position,
        "load_kifu": app_tools.load_kifu,
        "save_kifu": app_tools.save_kifu,
        "get_kifu": app_tools.get_kifu,
        "goto_ply": app_tools.goto_ply,
        "list_actions": app_tools.list_actions,
        "trigger_action": app_tools.trigger_action,
        "click_board_square": app_tools.click_board_square,
        "click_dialog_button": app_tools.click_dialog_button,
        "show_dock": app_tools.show_dock,
        "list_docks": app_tools.list_docks,
        "configure_dock": app_tools.configure_dock,
        "click_widget": app_tools.click_widget,
        "set_widget_value": app_tools.set_widget_value,
        "click_table_cell": app_tools.click_table_cell,
        "list_menu_actions": app_tools.list_menu_actions,
        "select_menu_action": app_tools.select_menu_action,
        "menu_favorites": app_tools.menu_favorites,
        "click_branch_node": app_tools.click_branch_node,
        "capture_screenshot": app_tools.capture_screenshot,
        "list_dialogs": app_tools.list_dialogs,
        "close_dialog": app_tools.close_dialog,
        "get_widget_text": app_tools.get_widget_text,
    }
    missing = {t.name for t in ALL_TOOLS} ^ set(handlers)
    assert not missing, f"tool/handler mismatch: {missing}"

    async def dispatch_tool(name: str, arguments: dict[str, Any] | None) -> ToolOutcome:
        handler = handlers.get(name)
        if handler is None:
            return _error_result("unknown_tool", f"Unknown tool {name!r}")
        try:
            return await handler(arguments or {})
        except ToolError as exc:
            log.info("tool %s failed: %s", name, exc)
            return _error_result(exc.code, exc.message, exc.data)
        except Exception as exc:  # pragma: no cover - defensive
            log.exception("tool %s crashed", name)
            return _error_result("internal_error", f"{type(exc).__name__}: {exc}")

    async def read_text_resource(uri: str) -> tuple[str, str]:
        try:
            if uri == "shogiboardq://position/current":
                pos = await client.call("position.get")
                return str(pos.get("sfen", "")), "text/plain"
            if uri == "shogiboardq://kifu/current":
                result = await client.call("kifu.get", {"format": "kif", "max_moves": 2000})
                return str(result.get("text", "")), "text/plain"
            if uri == "shogiboardq://engines":
                result = await run_cli(["list-engines"])
                return json.dumps(result.get("engines", []), ensure_ascii=False, indent=2), "application/json"
        except ToolError as exc:
            raise ValueError(f"{exc.code}: {exc.message}") from exc
        raise ValueError(f"Unknown resource: {uri}")

    return _build_lowlevel_server(dispatch_tool, read_text_resource), jobs, client


def _build_lowlevel_server(dispatch_tool: ToolDispatcher, read_text_resource: ResourceReader) -> Server:
    """Pass the handlers to the mcp 2.x low-level Server.

    The SDK serves both protocol eras on stdio: the stateless 2026-07-28 revision (``server/discover``)
    and the ``initialize`` handshake revisions 2024-11-05 to 2025-11-25.
    """

    async def list_tools(_ctx: Any, _params: Any) -> types.ListToolsResult:
        return types.ListToolsResult(tools=ALL_TOOLS)

    async def call_tool(_ctx: Any, params: types.CallToolRequestParams) -> types.CallToolResult:
        outcome = await dispatch_tool(params.name, params.arguments)
        if isinstance(outcome, types.CallToolResult):
            return outcome
        text, structured = outcome
        return types.CallToolResult(content=[types.TextContent(type="text", text=text)],
                                    structuredContent=structured)

    async def list_resources(_ctx: Any, _params: Any) -> types.ListResourcesResult:
        return types.ListResourcesResult(resources=RESOURCES)

    async def read_resource(_ctx: Any, params: types.ReadResourceRequestParams) -> types.ReadResourceResult:
        uri = str(params.uri)
        text, mime_type = await read_text_resource(uri)
        return types.ReadResourceResult(
            contents=[types.TextResourceContents(uri=uri, text=text, mimeType=mime_type)])  # type: ignore[arg-type]

    return Server(  # type: ignore[call-arg]
        "shogiboardq",
        version=__version__,
        instructions=INSTRUCTIONS,
        on_list_tools=list_tools,
        on_call_tool=call_tool,
        on_list_resources=list_resources,
        on_read_resource=read_resource,
    )


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
