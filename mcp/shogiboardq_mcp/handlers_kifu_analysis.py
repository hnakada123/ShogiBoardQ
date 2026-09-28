"""Whole-record analysis using the shared CLI position analyzer and job lifecycle."""

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
        _, record = await self.cli.convert_kifu({
            **{key: args[key] for key in ("input_path", "text", "input_format") if key in args},
            "output_format": "sfen", "max_chars": 1,
        })
        sfens = record["sfens"]
        moves = record["usi_moves"]
        start = int(args.get("from_ply", 0))
        end = int(args.get("to_ply", len(moves)))
        if start < 0 or end < start or end >= len(sfens) or end > len(moves):
            raise ToolError("invalid_argument", "Requested ply range is outside the main line")
        seconds = int(args.get("seconds_per_position", 1))
        multipv = int(args.get("multipv", 1))
        def steps():
            for ply in range(start, end + 1):
                # Build history lazily, keeping memory linear in the record length.
                command = ["analyze", "--engine", args["engine"], "--sfen", record["initial_sfen"],
                           "--seconds", str(seconds), "--multipv", str(multipv)]
                if ply:
                    command += ["--moves", " ".join(moves[:ply])]
                yield {"args": command, "ply": ply, "sfen": sfens[ply],
                       "played_move": moves[ply - 1] if ply else None}

        summary = {"engine": args["engine"], "from_ply": start, "to_ply": end,
                   "total_positions": end - start + 1, "seconds_per_position": seconds, "multipv": multipv,
                   "has_branches": record.get("has_branches", False), "warnings": record.get("warnings", [])}
        job = await self.jobs.start_sequence(steps(), summary)
        return f"Started kifu analysis {job.id}: positions {start} through {end}. Poll kifu_analysis_status.", job.status_base()

    async def status(self, args: dict[str, Any]):
        job = self.jobs.get(args["job_id"])
        if job.kind != "kifu_analysis":
            raise ToolError("invalid_argument", "job_id must refer to a kifu analysis job")
        positions = job.data.get("positions", [])
        offset = int(args.get("offset", 0))
        limit = int(args.get("max_positions", 100))
        max_pv = int(args.get("max_pv_moves", 12))
        page = []
        for position in positions[offset:offset + limit]:
            item = dict(position)
            item["lines"] = [{**line, "pv": line.get("pv", [])[:max_pv]} for line in position.get("lines", [])]
            page.append(item)
        result = {**job.status_base(), "completed": len(positions), "positions": page,
                  "current_ply": job.data.get("current_ply"), "offset": offset,
                  "next_offset": offset + len(page), "truncated": offset + len(page) < len(positions),
                  "partial": job.state != "finished"}
        text = [f"{job.id}: {job.state}, {len(positions)}/{job.summary['total_positions']} positions; "
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
