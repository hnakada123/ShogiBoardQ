"""Whole-record analysis: one `shogiboardq-cli analyze-kifu` job (one engine process for every position)."""

from __future__ import annotations

from typing import Any

from .errors import ToolError
from .formatting import format_lines
from .handlers_cli import CliTools
from .jobs import JobManager


class KifuAnalysisTools:
    def __init__(self, jobs: JobManager, cli: CliTools) -> None:
        self.jobs = jobs
        self.cli = cli

    async def analyze_kifu(self, args: dict[str, Any]):
        # Text is handed over as a temporary file (a long record does not fit on a command line); the CLI reads it
        # before it reports "started", and the file is removed then.
        with self.cli.kifu_input(args) as input_path:
            cli_args = self.cli.command("analyze-kifu", args, text=None, input_path=input_path)
            summary = {"engine": args["engine"], "seconds_per_position": int(args["seconds_per_position"]),
                       "multipv": int(args["multipv"])}
            job = await self.jobs.start("kifu_analysis", cli_args, summary, until="started")
        started = job.data.get("started", {})
        job.summary.update({key: started[key] for key in ("from_ply", "to_ply", "total_positions", "has_branches",
                                                          "warnings") if key in started})
        return (f"Started kifu analysis {job.id}: positions {started.get('from_ply')} through {started.get('to_ply')}. "
                "Poll kifu_analysis_status."), job.status_base()

    async def status(self, args: dict[str, Any]):
        job = self.jobs.get(args["job_id"])
        if job.kind != "kifu_analysis":
            raise ToolError("invalid_argument", "job_id must refer to a kifu analysis job")
        positions = job.data.get("positions", [])
        offset = int(args["offset"])
        limit = int(args["max_positions"])
        max_pv = int(args["max_pv_moves"])
        page = []
        for position in positions[offset:offset + limit]:
            item = {key: value for key, value in position.items() if key != "event"}
            item["lines"] = [{**line, "pv": line.get("pv", [])[:max_pv]} for line in position.get("lines", [])]
            page.append(item)
        result = {**job.status_base(), "completed": len(positions), "positions": page,
                  "current_ply": job.data.get("progress", {}).get("current_ply"), "offset": offset,
                  "next_offset": offset + len(page), "truncated": offset + len(page) < len(positions),
                  "partial": job.state != "finished"}
        text = [f"{job.id}: {job.state}, {len(positions)}/{job.summary.get('total_positions', '?')} positions; "
                f"returning {len(page)} from offset {offset}."]
        if job.error:
            text.append(f"Error: {job.error}")
        for position in page:
            text.append(f"Ply {position['ply']}, bestmove {position.get('bestmove', '?')} "
                        "(line scores from the side to move):")
            text.extend(format_lines(position.get("lines", []), max_pv))
        if result["truncated"]:
            text.append(f"More positions: use offset={result['next_offset']}.")
        return "\n".join(text), result
