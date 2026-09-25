"""Phase 1 tool handlers: everything that runs through ``shogiboardq-cli``.

Each handler returns ``(text, structured)`` where ``text`` is a short human
readable summary and ``structured`` matches the tool's outputSchema.
"""

from __future__ import annotations

import os
import tempfile
from pathlib import Path
from typing import Any

from . import formatting as fmt
from . import paths
from .cli_backend import run_cli
from .errors import ToolError
from .jobs import Job, JobManager

Result = tuple[str, dict[str, Any]]


class CliTools:
    def __init__(self, jobs: JobManager) -> None:
        self.jobs = jobs

    # ------------------------------------------------------------------ helpers
    @staticmethod
    def _position_args(args: dict[str, Any]) -> list[str]:
        out = ["--sfen", args.get("sfen") or "startpos"]
        moves = args.get("moves") or []
        if moves:
            out += ["--moves", " ".join(moves)]
        return out

    # ------------------------------------------------------------------ simple tools
    async def convert_kifu(self, args: dict[str, Any]) -> Result:
        cli_args = ["convert-kifu", "--output-format", args["output_format"],
                    "--input-format", args.get("input_format", "auto")]
        temp_path: Path | None = None
        if args.get("input_path"):
            cli_args += ["--input", str(paths.resolve_read_path(args["input_path"], "input_path"))]
        else:
            # Long records do not fit on a command line; hand the text over as a temporary file.
            suffix = {"auto": ".txt", "kif": ".kif", "ki2": ".ki2", "csa": ".csa", "jkf": ".jkf",
                      "usi": ".usi", "usen": ".usen"}[args.get("input_format", "auto")]
            fd, name = tempfile.mkstemp(prefix="shogiboardq-mcp-", suffix=suffix)
            with os.fdopen(fd, "w", encoding="utf-8") as fh:
                fh.write(args["text"])
            temp_path = Path(name)
            cli_args += ["--input", str(temp_path)]
        output_path = None
        if args.get("output_path"):
            output_path = paths.resolve_write_path(args["output_path"], bool(args.get("overwrite")))
            cli_args += ["--output", str(output_path)]
            if args.get("overwrite"):
                cli_args.append("--overwrite")
        try:
            result = await run_cli(cli_args, timeout=120)
        finally:
            if temp_path is not None:
                try:
                    temp_path.unlink()
                except OSError:
                    pass

        structured = {k: result[k] for k in ("input_format", "output_format", "initial_sfen", "ply_count",
                                              "usi_moves", "sfens", "moves", "game_info", "has_branches", "warnings")
                      if k in result}
        text_lines = [
            f"Converted {result['input_format'].upper()} → {result['output_format'].upper()}: "
            f"{result['ply_count']} moves from {'the initial position' if result['initial_sfen'].startswith('lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b -') else result['initial_sfen']}."
        ]
        if result.get("has_branches"):
            text_lines.append("The record has variations; only the main line is exported.")
        if result.get("warnings"):
            text_lines.append("Warnings: " + "; ".join(result["warnings"]))
        if output_path is not None:
            structured["output_path"] = str(output_path)
            structured["truncated"] = False
            text_lines.append(f"Written to {output_path}")
        else:
            text, truncated = fmt.truncate_text(result.get("text", ""), int(args.get("max_chars", 30000)))
            structured["text"] = text
            structured["truncated"] = truncated
            text_lines.append("")
            text_lines.append(text)
        return "\n".join(text_lines), structured

    async def validate_sfen(self, args: dict[str, Any]) -> Result:
        result = await run_cli(["validate-sfen", "--sfen", args["sfen"]])
        structured = {k: v for k, v in result.items() if k != "ok"}
        structured["sfen"] = args["sfen"]
        if result["valid"]:
            hands = []
            for side in ("black", "white"):
                counts = result.get("hands", {}).get(side, {})
                hands.append(f"{side}: " + (" ".join(f"{p}{n if n > 1 else ''}" for p, n in counts.items()) or "none"))
            text = (
                f"Valid. {'Black' if result['turn'] == 'b' else 'White'} to move"
                f"{' (in check)' if result.get('in_check') else ''}, {result.get('legal_move_count', '?')} legal moves. "
                f"Hands — {'; '.join(hands)}. Normalized: {result['normalized_sfen']}"
            )
        else:
            text = "Invalid SFEN: " + "; ".join(result.get("errors", []))
        return text, structured

    async def list_engines(self, args: dict[str, Any]) -> Result:
        result = await run_cli(["list-engines"])
        engines = result.get("engines", [])
        if not engines:
            text = "No USI engines are registered in ShogiBoardQ. Register engines via Settings > Engine Settings in the GUI."
        else:
            text = "Registered engines:\n" + "\n".join(
                f"  - {e['name']}" + ("" if e.get("exists", True) else " (executable missing)") + f" — {e.get('path', '')}"
                for e in engines
            )
        return text, {"engines": engines}

    async def render_board_image(self, args: dict[str, Any]) -> Result:
        output_path = paths.resolve_write_path(args["output_path"], bool(args.get("overwrite")))
        cli_args = ["render-board", "--sfen", args.get("sfen") or "startpos", "--output", str(output_path),
                    "--square-size", str(args.get("square_size", 50))]
        if args.get("flip"):
            cli_args.append("--flip")
        if args.get("last_move"):
            cli_args += ["--last-move", args["last_move"]]
        if args.get("black_name"):
            cli_args += ["--black-name", args["black_name"]]
        if args.get("white_name"):
            cli_args += ["--white-name", args["white_name"]]
        if args.get("overwrite"):
            cli_args.append("--overwrite")
        result = await run_cli(cli_args)
        structured = {"output_path": result["output_path"], "width": result["width"], "height": result["height"]}
        return f"Board image written to {result['output_path']} ({result['width']}x{result['height']} px).", structured

    async def verify_tsume(self, args: dict[str, Any]) -> Result:
        timeout_s = int(args.get("timeout_seconds", 15))
        cli_args = ["verify-tsume", "--engine", args["engine"], "--sfen", args["sfen"],
                    "--target-moves", str(args["target_moves"]), "--timeout-ms", str(timeout_s * 1000)]
        if not args.get("allow_final_move_alternatives", True):
            cli_args.append("--no-final-alternatives")
        result = await run_cli(cli_args, timeout=timeout_s + 30)
        structured = {k: result[k] for k in ("status", "pv", "queries", "elapsed_ms", "sfen", "target_moves") if k in result}
        explanations = {
            "unique": f"Unique solution in {args['target_moves']} plies: {' '.join(result.get('pv', []))}",
            "multiple": "Not unique: another attacking move also mates on the main line (a cook).",
            "nomate": "No mate: the engine proved the position cannot be mated.",
            "wrong_length": "The shortest mate has a different length than target_moves.",
            "unknown": "Undecided within the time budget; raise timeout_seconds or use a faster engine.",
            "invalid": "The position is not a valid tsume position.",
        }
        text = f"{result['status']}: {explanations.get(result['status'], '')} ({result.get('queries', 0)} engine queries, {result.get('elapsed_ms', 0) / 1000:.1f} s)"
        return text, structured

    # ------------------------------------------------------------------ jobs
    async def analyze_position(self, args: dict[str, Any]) -> Result:
        seconds = int(args.get("seconds", 10))
        multipv = int(args.get("multipv", 1))
        cli_args = ["analyze", "--engine", args["engine"], *self._position_args(args),
                    "--seconds", str(seconds), "--multipv", str(multipv)]
        summary = {"engine": args["engine"], "seconds": seconds, "multipv": multipv,
                   "sfen": args.get("sfen") or "startpos", "moves": args.get("moves") or []}
        job = await self.jobs.start("analysis", cli_args, summary)
        return (
            f"Started analysis job {job.id} with {args['engine']} for {seconds} s (MultiPV {multipv}). "
            f"Call analysis_status or analysis_result with this job_id.",
            {"job_id": job.id, "state": job.state, "engine": args["engine"], "seconds": seconds, "multipv": multipv},
        )

    def _analysis_status(self, job: Job, max_pv: int, final: bool) -> Result:
        status = job.status_base()
        data = job.data
        started = data.get("started", {})
        if started.get("sfen"):
            status["sfen"] = started["sfen"]
        for key in ("depth", "nodes", "nps"):
            if key in data:
                status[key] = data[key]
        if job.result is not None and job.result.get("lines"):
            lines_src = job.result["lines"]
        else:
            lines_src = [data.get("lines", {})[k] for k in sorted(data.get("lines", {}))]
        lines = []
        for line in lines_src:
            entry = {k: v for k, v in line.items() if k not in ("event",)}
            pv = list(entry.get("pv", []))[:max_pv]
            entry["pv"] = pv
            entry["pv_text"] = " ".join(pv)
            lines.append(entry)
        status["lines"] = lines
        if job.result is not None:
            if job.result.get("bestmove"):
                status["bestmove"] = job.result["bestmove"]
            if job.result.get("ponder"):
                status["ponder"] = job.result["ponder"]
        status["partial"] = job.is_active()
        text = [fmt.format_job_header(status)]
        if status.get("bestmove"):
            text.append(f"bestmove {status['bestmove']}" + (f" ponder {status['ponder']}" if status.get("ponder") else ""))
        elif final and job.is_active():
            text.append("still running; lines below are provisional")
        if lines:
            text.extend(fmt.format_lines(lines, max_pv))
        elif job.is_active():
            text.append("  no principal variation received yet")
        if job.error:
            text.append(f"error: {job.error}")
        return "\n".join(text), status

    async def analysis_status(self, args: dict[str, Any]) -> Result:
        return self._analysis_status(self.jobs.get(args["job_id"]), int(args.get("max_pv_moves", 12)), final=False)

    async def analysis_result(self, args: dict[str, Any]) -> Result:
        return self._analysis_status(self.jobs.get(args["job_id"]), int(args.get("max_pv_moves", 12)), final=True)

    async def search_mate(self, args: dict[str, Any]) -> Result:
        seconds = int(args.get("seconds", 10))
        cli_args = ["mate", "--engine", args["engine"], *self._position_args(args), "--seconds", str(seconds)]
        summary = {"engine": args["engine"], "seconds": seconds, "sfen": args["sfen"], "moves": args.get("moves") or []}
        job = await self.jobs.start("mate", cli_args, summary)
        return (
            f"Started mate search job {job.id} with {args['engine']} for up to {seconds} s. Poll mate_status.",
            {"job_id": job.id, "state": job.state, "engine": args["engine"]},
        )

    async def mate_status(self, args: dict[str, Any]) -> Result:
        job = self.jobs.get(args["job_id"])
        status = job.status_base()
        started = job.data.get("started", {})
        if started.get("sfen"):
            status["sfen"] = started["sfen"]
        text = [fmt.format_job_header(status)]
        if job.result is not None:
            status["status"] = job.result.get("status")
            status["pv"] = job.result.get("pv", [])
            status["plies"] = job.result.get("plies", len(status["pv"]))
            if status["status"] == "mate":
                text.append(f"mate in {status['plies']} plies: {' '.join(status['pv'])}")
            elif status["status"] == "nomate":
                text.append("no mate (proved by the engine)")
            elif status["status"] == "notimplemented":
                text.append("the engine does not support go mate; choose another engine (e.g. KomoringHeights)")
            else:
                text.append("unknown: the engine did not find a mate within the time limit")
        elif job.is_active():
            text.append("searching…")
        if job.error:
            text.append(f"error: {job.error}")
        return "\n".join(text), status

    async def generate_tsume(self, args: dict[str, Any]) -> Result:
        target = int(args.get("target_moves", 3))
        if target % 2 == 0:
            raise ToolError("invalid_argument", "target_moves must be odd (1, 3, 5, …)")
        cli_args = [
            "generate-tsume", "--engine", args["engine"],
            "--target-moves", str(target),
            "--max-positions", str(int(args.get("max_positions", 5))),
            "--timeout-ms", str(int(args.get("timeout_ms", 5000))),
            "--max-attack", str(int(args.get("max_attack_pieces", 4))),
            "--max-defend", str(int(args.get("max_defend_pieces", 1))),
            "--attack-range", str(int(args.get("attack_range", 3))),
        ]
        if not args.get("add_remaining_to_defender_hand", True):
            cli_args.append("--no-remaining-to-hand")
        if not args.get("allow_final_move_alternatives", True):
            cli_args.append("--no-final-alternatives")
        summary = {"engine": args["engine"], "target_moves": target,
                   "max_positions": int(args.get("max_positions", 5))}
        job = await self.jobs.start("tsume_generation", cli_args, summary)
        return (
            f"Started tsume generation job {job.id} ({target}-ply problems, up to {summary['max_positions']}) with "
            f"{args['engine']}. Poll tsume_generation_status; stop with stop_tsume_generation.",
            {"job_id": job.id, "state": job.state, "engine": args["engine"]},
        )

    def _tsume_status(self, job: Job, max_positions: int) -> Result:
        status = job.status_base()
        progress = job.result if job.result is not None else job.data.get("progress", {})
        for key in ("generated", "found", "rejected", "inconclusive"):
            status[key] = int(progress.get(key, 0)) if progress else 0
        positions = job.data.get("positions", [])
        status["found"] = max(status["found"], len(positions))
        status["positions"] = [{"index": p.get("index"), "sfen": p["sfen"], "pv": p.get("pv", [])} for p in positions[:max_positions]]
        if job.result is not None and "stopped" in job.result:
            status["stopped"] = bool(job.result["stopped"])
        text = [fmt.format_job_header(status),
                f"found {status['found']} / generated {status['generated']} candidates "
                f"(rejected by uniqueness check {status['rejected']}, inconclusive {status['inconclusive']})"]
        for p in status["positions"]:
            text.append(f"  {p['index']}. {p['sfen']}  ({' '.join(p['pv'])})")
        if len(positions) > max_positions:
            text.append(f"  … {len(positions) - max_positions} more (raise max_positions)")
        if job.error:
            text.append(f"error: {job.error}")
        return "\n".join(text), status

    async def tsume_generation_status(self, args: dict[str, Any]) -> Result:
        return self._tsume_status(self.jobs.get(args["job_id"]), int(args.get("max_positions", 20)))

    async def stop_tsume_generation(self, args: dict[str, Any]) -> Result:
        job = await self.jobs.stop(args["job_id"])
        return self._tsume_status(job, 100)

    async def list_jobs(self, args: dict[str, Any]) -> Result:
        jobs = [j.status_base() for j in self.jobs.list()]
        text = "No jobs." if not jobs else "\n".join(fmt.format_job_header(j) for j in jobs)
        return text, {"jobs": jobs}

    async def cancel_job(self, args: dict[str, Any]) -> Result:
        job = await self.jobs.stop(args["job_id"])
        return f"job {job.id} is now {job.state}", {"job_id": job.id, "state": job.state}
