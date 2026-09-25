"""Phase 2 tool handlers: operations on a running ``ShogiBoardQ --automation``."""

from __future__ import annotations

from typing import Any

from . import formatting as fmt
from . import paths
from .app_client import AppClient

Result = tuple[str, dict[str, Any]]


class AppTools:
    def __init__(self, client: AppClient) -> None:
        self.client = client

    async def get_app_state(self, args: dict[str, Any]) -> Result:
        state = await self.client.call("app.state")
        dialogs = state.get("dialogs") or []
        text = (
            f"ShogiBoardQ is {state.get('ui_state', '?')} (play mode {state.get('play_mode', '?')}), "
            f"ply {state.get('current_ply', 0)}/{state.get('total_plies', 0)}"
            + (f", file {state['kifu_file']}" if state.get("kifu_file") else ", no file")
            + (", unsaved changes" if state.get("dirty") else "")
            + (". Open dialogs: " + ", ".join(d.get("title") or d.get("object_name", "?") for d in dialogs) if dialogs else ".")
        )
        return text, state

    async def get_position(self, args: dict[str, Any]) -> Result:
        pos = await self.client.call("position.get")
        moves = pos.get("moves") or []
        text = f"SFEN: {pos.get('sfen')}\nply {pos.get('ply', 0)}"
        if moves:
            text += f", moves from start: {fmt.pv_text(moves, 60)}"
        return text, pos

    async def set_position(self, args: dict[str, Any]) -> Result:
        result = await self.client.call("position.set", {"sfen": args["sfen"], "discard_unsaved": bool(args.get("discard_unsaved"))})
        return f"Position set: {result.get('sfen')}", result

    async def load_kifu(self, args: dict[str, Any]) -> Result:
        params: dict[str, Any] = {"discard_unsaved": bool(args.get("discard_unsaved"))}
        if args.get("path"):
            params["path"] = str(paths.resolve_read_path(args["path"], "path"))
        else:
            params["text"] = args["text"]
        result = await self.client.call("kifu.load", params, timeout=60)
        text = f"Loaded {result.get('total_plies', 0)} plies"
        if result.get("kifu_file"):
            text += f" from {result['kifu_file']}"
        return text + ".", result

    async def save_kifu(self, args: dict[str, Any]) -> Result:
        path = paths.resolve_write_path(args["path"], bool(args.get("overwrite")), "path")
        result = await self.client.call("kifu.save", {"path": str(path), "overwrite": bool(args.get("overwrite"))})
        return f"Saved {result.get('format', '')} record to {result.get('path')}", result

    async def get_kifu(self, args: dict[str, Any]) -> Result:
        params = {
            "format": args.get("format", "moves"),
            "from_ply": int(args.get("from_ply", 1)),
            "max_moves": int(args.get("max_moves", 200)),
        }
        result = await self.client.call("kifu.get", params)
        if params["format"] == "moves":
            moves = result.get("moves") or []
            lines = [f"{result.get('total_plies', 0)} plies total; showing {len(moves)} from ply {params['from_ply']}."]
            for m in moves:
                extra = ""
                if m.get("time"):
                    extra += f"  {m['time']}"
                if m.get("comment"):
                    extra += f"  # {m['comment']}"
                lines.append(f"{m.get('ply'):>4} {m.get('text', '')} ({m.get('usi', '')}){extra}")
            return "\n".join(lines), result
        text, truncated = fmt.truncate_text(result.get("text", ""), int(args.get("max_chars", 30000)))
        result["text"] = text
        result["truncated"] = truncated
        return text, result

    async def goto_ply(self, args: dict[str, Any]) -> Result:
        result = await self.client.call("kifu.goto", {"ply": int(args["ply"])})
        return f"Now at ply {result.get('ply')}: {result.get('sfen', '')}", result

    async def list_actions(self, args: dict[str, Any]) -> Result:
        result = await self.client.call("action.list")
        actions = result.get("actions", [])
        lines = [
            f"  {a['name']:<32} {a.get('text', '')}"
            + ("" if a.get("enabled", True) else " [disabled]")
            + (" [checked]" if a.get("checked") else "")
            for a in actions
        ]
        return "Allowed actions:\n" + "\n".join(lines), result

    async def trigger_action(self, args: dict[str, Any]) -> Result:
        result = await self.client.call("action.trigger", {"name": args["name"]})
        return f"Triggered {result.get('name')}.", result

    async def capture_screenshot(self, args: dict[str, Any]) -> Result:
        output_dir = paths.resolve_output_dir(args.get("output_dir"))
        result = await self.client.call("screenshot.capture", {"target": args.get("target", "main"), "output_dir": str(output_dir)})
        return f"Screenshot saved to {result.get('path')} ({result.get('width')}x{result.get('height')} px).", result

    async def list_dialogs(self, args: dict[str, Any]) -> Result:
        result = await self.client.call("dialog.list")
        windows = result.get("windows", [])
        lines = [
            f"  {w.get('class', '?')} {w.get('object_name') or '(unnamed)'} — \"{w.get('title', '')}\""
            + (" [modal]" if w.get("modal") else "") + (" [active]" if w.get("active") else "")
            for w in windows
        ]
        return ("Open windows:\n" + "\n".join(lines)) if lines else "No windows are open.", result

    async def close_dialog(self, args: dict[str, Any]) -> Result:
        result = await self.client.call("dialog.close", {"dialog": args["dialog"]})
        return f"Closed {result.get('object_name') or result.get('title') or args['dialog']}.", result

    async def get_widget_text(self, args: dict[str, Any]) -> Result:
        params: dict[str, Any] = {"max_rows": int(args.get("max_rows", 50))}
        if args.get("dialog"):
            params["dialog"] = args["dialog"]
        if args.get("widget"):
            params["widget"] = args["widget"]
        result = await self.client.call("widget.text", params)
        lines = []
        for w in result.get("widgets", []):
            name = w.get("object_name") or "(unnamed)"
            if "rows" in w:
                lines.append(f"{w.get('class')} {name}: {len(w['rows'])} rows")
                for row in w["rows"]:
                    lines.append("    " + " | ".join(str(c) for c in row))
            elif "items" in w:
                lines.append(f"{w.get('class')} {name}: " + ", ".join(str(i) for i in w["items"]))
            else:
                lines.append(f"{w.get('class')} {name}: {w.get('text', '')}")
        return "\n".join(lines) if lines else "No readable widgets found.", result
