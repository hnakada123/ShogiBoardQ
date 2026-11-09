"""Human readable summaries for tool results (the ``content`` text)."""

from __future__ import annotations

from typing import Any


def score_text(line: dict[str, Any]) -> str:
    if "score_mate" in line:
        mate = line["score_mate"]
        return f"mate {mate:+d}" if isinstance(mate, int) and mate != 0 else "mate"
    if "score_cp" in line:
        bound = {"lower": "≥", "upper": "≤"}.get(line.get("bound", ""), "")
        return f"{bound}{line['score_cp']:+d} cp"
    return "?"


def pv_text(pv: list[str], max_moves: int) -> str:
    if not pv:
        return "-"
    shown = " ".join(pv[:max_moves])
    if len(pv) > max_moves:
        shown += f" … (+{len(pv) - max_moves})"
    return shown


def format_lines(lines: list[dict[str, Any]], max_moves: int) -> list[str]:
    out = []
    for line in lines:
        out.append(
            f"  #{line.get('multipv', 1)} {score_text(line)} depth {line.get('depth', '?')}: "
            f"{pv_text(line.get('pv', []), max_moves)}"
        )
    return out


def format_job_header(status: dict[str, Any]) -> str:
    return f"job {status['job_id']} [{status['state']}] {status.get('kind', '')} ({status['elapsed_ms'] / 1000:.1f} s)"


def truncate_text(text: str, max_chars: int) -> tuple[str, bool]:
    if max_chars <= 0 or len(text) <= max_chars:
        return text, False
    return text[:max_chars] + f"\n…[truncated, {len(text) - max_chars} more characters]", True


# Fields of a record written by its author. They can be of any length, so together they share the max_chars budget.
_FREE_TEXT_KEYS = {"comment", "bookmark", "opening_comment", "opening_bookmark", "value", "key"}
_LIST_OF_FREE_TEXT_KEYS = {"warnings"}
# One move's text is short unless a record invents a long end-of-game word.
_MAX_MOVE_TEXT_CHARS = 200


def cap_free_text(value: Any, budget: int) -> tuple[Any, bool]:
    """Cap the free-form strings of a result (comments, game information, warnings) to ``budget`` characters together.

    max_chars must bound what an agent receives from a crafted record, not only the exported text. The result keeps
    its shape; each cut string ends with a truncation mark.
    """
    remaining = max(0, budget)
    cut = False

    def trimmed(text: str) -> str:
        nonlocal remaining, cut
        if len(text) <= remaining:
            remaining -= len(text)
            return text
        cut = True
        shown = text[:remaining]
        remaining = 0
        return shown + "…[truncated]"

    def walk(node: Any, key: str | None) -> Any:
        if isinstance(node, dict):
            return {k: walk(v, k) for k, v in node.items()}
        if isinstance(node, list):
            if key in _LIST_OF_FREE_TEXT_KEYS:
                return [trimmed(item) if isinstance(item, str) else walk(item, None) for item in node]
            return [walk(item, key) for item in node]
        if isinstance(node, str):
            if key in _FREE_TEXT_KEYS:
                return trimmed(node)
            if key == "text" and len(node) > _MAX_MOVE_TEXT_CHARS:
                nonlocal cut
                cut = True
                return node[:_MAX_MOVE_TEXT_CHARS] + "…[truncated]"
        return node

    return walk(value, None), cut


_UNTRUSTED_OPEN = "<untrusted-data>"
_UNTRUSTED_CLOSE = "</untrusted-data>"


def untrusted(text: str) -> str:
    """Mark text written by a third party (a record's comments, a file's contents) for the model reading the result.

    The closing mark inside the text is neutralized, so the text cannot end the marked region itself. This lowers the
    risk of prompt injection; it cannot remove it.
    """
    if not text:
        return text
    safe = text.replace(_UNTRUSTED_CLOSE, "</untrusted-data\u200b>").replace(_UNTRUSTED_OPEN, "<untrusted-data\u200b>")
    return f"{_UNTRUSTED_OPEN}{safe}{_UNTRUSTED_CLOSE}"
