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
