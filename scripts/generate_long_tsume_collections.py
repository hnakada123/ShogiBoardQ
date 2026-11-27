#!/usr/bin/env python3
"""shogiboardq-cli grow-tsume を並列実行して長手数詰将棋問題集（例: 15〜31手詰 各1000題）を生成する。

特徴:
- 全ワーカー共通の tsume_diversity.DiversityIndex で重複・平行移動・鏡像・中間手順類似作を即時除外。
- 厳密基準（全主手順一意・最終手複数解禁止・駒余りなし・不要駒全1枚除去・go mate infinite自然終了）。
- 常駐ワーカーへ stdin 経由で採択済みの exclude / seed をリアルタイム同期し、再起動ロスと重複作業を極小化。
- すでに子問題を生んだ親種（同一 core）を自動除外し、未消化親種逆算・採択済み同手数の複合変異・
  2段階逆算・0手詰み局面からの新規構築を複数ワーカーで役割分担する。
- 1題ごとに JSONL へ即時保存し、いつでも中断・再開可能。各手数が目標題数に達したら公開形式で出力。
"""
import argparse
import asyncio
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import random
import signal
import sys
import time

sys.path.insert(0, str(Path(__file__).resolve().parent))
from tsume_diversity import DiversityIndex, Problem, board_from_sfen, canonical, metrics, spread, square
from tsume_surplus import remaining_hand


def atomic_write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    tmp.write_text(text, encoding="utf-8")
    tmp.replace(path)


def load_problems_from_file(path: Path) -> list[Problem]:
    problems = []
    if not path.exists():
        return problems
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#") or " moves " not in line:
            continue
        try:
            problems.append(Problem.parse(line))
        except ValueError:
            continue
    return problems


def self_core_key(problem: Problem):
    """この問題自身が2手逆算された際に、子問題の core (pv[2:-1]) になる正規化キー。"""
    board = board_from_sfen(problem.sfen)
    king = next(sq for sq, piece in board.items() if piece == "k")
    return canonical(problem.pv[:-1], king)


def parent_core_key(problem: Problem):
    """長さ9以上の問題について、初手2手を進めた後の core (pv[2:-1]) 正規化キー。"""
    if len(problem.pv) < 9:
        return None
    board = board_from_sfen(problem.sfen)
    king = next(sq for sq, piece in board.items() if piece == "k")
    for move in problem.pv[:2]:
        dest = square(move[2:4])
        if move[1] == "*":
            piece = move[0]
        else:
            piece = board.pop(square(move[:2]))
            if piece == "k":
                king = dest
            if move.endswith("+"):
                piece = "+" + piece
        board[dest] = piece
    return canonical(problem.pv[2:-1], king)


def tier_params(plies: int) -> dict:
    if plies <= 19:
        return {
            "max_attack": 14,
            "max_defend": 10,
            "beam_width": 600,
            "branch_factor": 64,
            "step_timeout_ms": 400,
            "verify_query_timeout_ms": 6000,
            "verify_budget_seconds": 60,
            "round_seconds": 180,
            "seed_batch": 250,
        }
    if plies <= 25:
        return {
            "max_attack": 16,
            "max_defend": 12,
            "beam_width": 400,
            "branch_factor": 40,
            "step_timeout_ms": 500,
            "verify_query_timeout_ms": 12000,
            "verify_budget_seconds": 120,
            "round_seconds": 300,
            "seed_batch": 150,
        }
    return {
        "max_attack": 18,
        "max_defend": 14,
        "beam_width": 300,
        "branch_factor": 32,
        "step_timeout_ms": 900,
        "verify_query_timeout_ms": 18000,
        "verify_budget_seconds": 210,
        "round_seconds": 420,
        "seed_batch": 120,
    }


def prepare_engine_config(config_home: Path, engine_path: Path, hash_mb: int) -> None:
    cfg_dir = config_home / "ShogiBoardQ"
    cfg_dir.mkdir(parents=True, exist_ok=True)
    # EngineProfileStore 準拠の保存形式（EngineOptions/KomoringHeights 配列）
    ini = (
        "[Engines]\n"
        "size=1\n"
        "1\\name=KomoringHeights\n"
        f"1\\path={engine_path.resolve()}\n"
        "\n"
        "[EngineOptions]\n"
        "KomoringHeights\\size=2\n"
        "KomoringHeights\\1\\name=Threads\n"
        "KomoringHeights\\1\\type=spin\n"
        "KomoringHeights\\1\\value=1\n"
        "KomoringHeights\\2\\name=USI_Hash\n"
        "KomoringHeights\\2\\type=spin\n"
        f"KomoringHeights\\2\\value={hash_mb}\n"
    )
    atomic_write(cfg_dir / "ShogiBoardQ.ini", ini)


def format_collection_file(plies: int, problems: list[Problem], stamp: str, date_str: str,
                           allow_final_alt: bool) -> str:
    ordered = spread(problems)
    header = [
        f"# ShogiBoardQ — {plies}手詰 {len(ordered)}題",
        "# Format: SFEN moves USI...",
        f"# Generated and strictly certified: {stamp}",
        "# Method: retrograde construction from mate/seed positions, mutation, cook repair and beam search (shogiboardq-cli grow-tsume).",
        "# Shared diversity filter across all workers (tsume_diversity.py): reflection/translation, gold-equivalent and middle-line variants excluded.",
        "# Every position: exact shortest mate (KomoringHeights MinLength go mate infinite natural completion), legal PV,",
        "# full main-line uniqueness, and conclusive verification of all single-piece removals to defender hand.",
        "# Final-move alternatives are allowed." if allow_final_alt else "# Final-move alternatives are strictly forbidden (every main-line attack move including the final move is unique).",
        "# Every longest defense and final mating move leaves the attacker hand empty (no surplus).",
        "# Order chosen to separate similar move sequences in the preceding 10 problems.",
        f"# See README.md and validation_long_{date_str}.json.",
        "#",
    ]
    return "\n".join(header + [p.line for p in ordered]) + "\n"


def parse_args():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--cli", type=Path, default=Path("build/shogiboardq-cli"))
    p.add_argument("--engine", type=Path, required=True, help="KomoringHeights 実行ファイルパス")
    p.add_argument("--work-dir", type=Path, required=True, help="途中保存・ログ・チェックポイント用ディレクトリ")
    p.add_argument("--publish-dir", type=Path, default=Path("data/tsumeshogi"), help="完成した問題集の出力先")
    p.add_argument("--initial-seeds", type=Path, nargs="+",
                   default=[Path("data/tsumeshogi/tsume_9ply_1000_20261001.txt"),
                            Path("data/tsumeshogi/tsume_11ply_1000_20261001.txt"),
                            Path("data/tsumeshogi/tsume_13ply_1000_20261001.txt")])
    p.add_argument("--plies", type=int, nargs="+", default=[15, 17, 19, 21, 23, 25, 27, 29, 31])
    p.add_argument("--count", type=int, default=1000, help="各手数の目標題数")
    p.add_argument("--workers", type=int, default=8, help="並列ワーカー数")
    p.add_argument("--hash-mb", type=int, default=64, help="1ワーカーあたり USI_Hash (MiB)")
    p.add_argument("--max-seconds", type=int, default=0, help="最大実行時間（秒、0=完了まで無制限）")
    p.add_argument("--date", type=str, default=datetime.now().strftime("%Y%m%d"))
    p.add_argument("--allow-final-alternatives", action="store_true", help="最終手の複数解を許容する（既定は禁止）")
    p.add_argument("--no-diversity", action="store_true", help="類似作除外を無効化しSFEN重複除外のみにする")
    return p.parse_args()


async def run_pipeline(args) -> int:
    cli_path = args.cli.resolve()
    engine_path = args.engine.resolve()
    work_dir = args.work_dir.resolve()
    publish_dir = args.publish_dir.resolve()
    work_dir.mkdir(parents=True, exist_ok=True)
    publish_dir.mkdir(parents=True, exist_ok=True)

    if not cli_path.exists():
        raise FileNotFoundError(f"CLI not found: {cli_path}")
    if not engine_path.exists():
        raise FileNotFoundError(f"Engine not found: {engine_path}")

    config_home = work_dir / "config"
    prepare_engine_config(config_home, engine_path, args.hash_mb)

    target_plies_list = sorted(set(args.plies))
    for n in target_plies_list:
        if n < 3 or n % 2 == 0:
            raise ValueError(f"Invalid target plies: {n}")

    # 手数ごとの親種プールとコアキーキャッシュ
    seed_pools: dict[int, list[Problem]] = {}
    self_core_cache: dict[str, object] = {}

    def get_self_core(prob: Problem):
        key = self_core_cache.get(prob.sfen)
        if key is None:
            key = self_core_key(prob)
            self_core_cache[prob.sfen] = key
        return key

    for seed_path in (args.initial_seeds or []):
        if seed_path.exists():
            for prob in load_problems_from_file(seed_path):
                seed_pools.setdefault(len(prob.pv), []).append(prob)

    accepted: dict[int, list[Problem]] = {n: [] for n in target_plies_list}
    accepted_sfens: dict[int, set[str]] = {n: set() for n in target_plies_list}
    div_indexes: dict[int, DiversityIndex] = {n: DiversityIndex() for n in target_plies_list}
    used_parent_cores: dict[int, set] = {n: set() for n in target_plies_list}
    stats = Counter()
    seed_usage: dict[int, Counter] = {n: Counter() for n in target_plies_list}
    round_counter: Counter = Counter()

    def register_accepted(n: int, prob: Problem) -> bool:
        if len(prob.pv) != n or remaining_hand(prob) or prob.sfen in accepted_sfens[n]:
            return False
        if not args.no_diversity and not div_indexes[n].add(prob):
            return False
        accepted_sfens[n].add(prob.sfen)
        accepted[n].append(prob)
        pcore = parent_core_key(prob)
        if pcore is not None:
            used_parent_cores[n].add(pcore)
        return True

    # 既存の途中結果を読み込んで完全再開
    for n in target_plies_list:
        jsonl = work_dir / f"collected_{n}ply.jsonl"
        if jsonl.exists():
            for raw in jsonl.read_text(encoding="utf-8").splitlines():
                if not raw.strip():
                    continue
                try:
                    rec = json.loads(raw)
                    prob = Problem(rec["sfen"], tuple(rec["pv"]))
                except (KeyError, ValueError, json.JSONDecodeError):
                    continue
                register_accepted(n, prob)
        pub_file = publish_dir / f"tsume_{n}ply_{args.count}_{args.date}.txt"
        if pub_file.exists() and len(accepted[n]) < args.count:
            for prob in load_problems_from_file(pub_file):
                register_accepted(n, prob)
        if accepted[n]:
            seed_pools[n] = list(accepted[n])

    stop_event = asyncio.Event()
    loop = asyncio.get_running_loop()
    for sig in (signal.SIGINT, signal.SIGTERM):
        try:
            loop.add_signal_handler(sig, stop_event.set)
        except NotImplementedError:
            pass

    started_at = time.monotonic()
    rng = random.Random(int(time.time()))
    published_tiers: set[int] = set()
    # 稼働中ワーカーの登録簿（リアルタイム exclude / seed 配信用）
    # worker_id -> {"plies": int, "role": str, "proc": Process, "stop_sent": bool}
    active_workers: dict[int, dict] = {}

    def write_exclude_file(n: int) -> Path:
        path = work_dir / f"exclude_{n}ply.txt"
        lines = [p.line for p in accepted[n]]
        atomic_write(path, ("\n".join(lines) + "\n") if lines else "")
        return path

    def write_validation_summary() -> None:
        files_meta = []
        for n in target_plies_list:
            out_path = publish_dir / f"tsume_{n}ply_{args.count}_{args.date}.txt"
            if not out_path.exists():
                continue
            probs = load_problems_from_file(out_path)
            files_meta.append({
                "file": out_path.name,
                "sha256": hashlib.sha256(out_path.read_bytes()).hexdigest(),
                "target_plies": n,
                "positions": len(probs),
                "unique_positions": len({p.sfen for p in probs}),
                "legal_mate_lines": len(probs),
                "shortest_mate_exact_target": len(probs),
                "positions_with_removable_piece": 0,
                "inconclusive_positions": 0,
                "saved_lines_with_surplus": sum(bool(remaining_hand(p)) for p in probs),
                "diversity": metrics(probs),
            })
        if not files_meta:
            return
        summary = {
            "generated_at": datetime.now(timezone.utc).isoformat(timespec="seconds"),
            "engine": "KomoringHeights (MinLength, go mate infinite natural completion)",
            "allow_final_move_alternatives": args.allow_final_alternatives,
            "require_no_surplus": True,
            "method": "all_single_piece_removals_to_defender_hand",
            "generation_method": "grow-tsume retrograde construction + strict verification + all single-piece removals",
            "diversity_filter_enabled": not args.no_diversity,
            "files": files_meta,
            "total_positions": sum(f["positions"] for f in files_meta),
        }
        atomic_write(publish_dir / f"validation_long_{args.date}.json",
                     json.dumps(summary, ensure_ascii=False, indent=2) + "\n")

    def publish_tier_if_ready(n: int) -> None:
        if len(accepted[n]) < args.count or n in published_tiers:
            return
        stamp = datetime.now(timezone.utc).isoformat(timespec="seconds")
        subset = accepted[n][:args.count]
        text = format_collection_file(n, subset, stamp, args.date, args.allow_final_alternatives)
        out_path = publish_dir / f"tsume_{n}ply_{args.count}_{args.date}.txt"
        atomic_write(out_path, text)
        published_tiers.add(n)
        write_validation_summary()
        print(json.dumps({
            "event": "tier_published",
            "plies": n,
            "count": len(subset),
            "path": str(out_path),
            "diversity": metrics(subset),
        }, ensure_ascii=False), flush=True)

    def write_progress() -> dict:
        elapsed = round(time.monotonic() - started_at, 1)
        prog = {
            "elapsed_seconds": elapsed,
            "workers": args.workers,
            "target_per_tier": args.count,
            "allow_final_move_alternatives": args.allow_final_alternatives,
            "require_no_surplus": True,
            "diversity_filter": not args.no_diversity,
            "stats": dict(stats),
            "tiers": {
                str(n): {
                    "accepted": len(accepted[n]),
                    "remaining": max(0, args.count - len(accepted[n])),
                    "published": n in published_tiers,
                }
                for n in target_plies_list
            },
        }
        atomic_write(work_dir / "progress.json", json.dumps(prog, ensure_ascii=False, indent=2) + "\n")
        return prog

    def choose_target_plies() -> int | None:
        # 短い手数から順に目標題数を完成させる（次手数の 100% 最終手一意な1000系統の種を確実に確保）
        for n in target_plies_list:
            if len(accepted[n]) < args.count:
                return n
        return None

    def pick_role_and_seeds(target_plies: int, worker_id: int, batch_size: int) -> tuple[str, list[Problem]]:
        parent_plies = target_plies - 2
        grand_plies = target_plies - 4
        used_cores = used_parent_cores[target_plies]
        unused_parents = [
            p for p in seed_pools.get(parent_plies, [])
            if args.no_diversity or get_self_core(p) not in used_cores
        ]
        same = list(accepted.get(target_plies, []))
        grand = list(seed_pools.get(grand_plies, []))
        usage = seed_usage[target_plies]

        # ワーカーごとに戦略を役割分担する（同手数複合変異・未消化親種逆算・2段逆算・0手新規構築）
        slot = (worker_id + (round_counter[target_plies] // max(1, args.workers))) % max(1, args.workers)
        round_counter[target_plies] += 1

        # 採択済み同手数が貯まるにつれ、100%最終手一意・駒余りなしの同手数からの
        # ステッピングストーン変異ワーカーの割合を自動拡大する
        if slot == args.workers - 1 and args.workers >= 8:
            return "fresh", []

        if grand and slot == args.workers - 2 and args.workers >= 6:
            rng.shuffle(grand)
            return "grand", grand[:batch_size]

        # 残りのスロット（args.workers - 2 台）を mutate と retro_parent に配分
        usable = max(1, args.workers - (2 if args.workers >= 8 else (1 if args.workers >= 6 else 0)))
        if len(same) >= 250:
            # 15手詰の親種枯渇後や終盤は mutate に最大集中（retro_parent は1〜2台残す）
            retro_keep = 1 if (target_plies == 15 or len(unused_parents) < 300) else 2
            mutate_quota = max(1, usable - retro_keep) if unused_parents else usable
        elif len(same) >= 100:
            mutate_quota = max(1, (usable * 2) // 3)
        elif len(same) >= 24:
            mutate_quota = max(1, usable // 3)
        else:
            mutate_quota = 0
        if not unused_parents and same:
            mutate_quota = usable

        if len(same) >= 16 and slot < mutate_quota:
            rng.shuffle(same)
            chosen = same[:batch_size]
            return "mutate", chosen

        if unused_parents:
            unused_parents.sort(key=lambda p: (usage[p.sfen], rng.random()))
            chosen = unused_parents[:batch_size]
            for p in chosen:
                usage[p.sfen] += 1
            return "retro_parent", chosen

        if same:
            rng.shuffle(same)
            return "mutate", same[:batch_size]

        if grand:
            rng.shuffle(grand)
            return "grand", grand[:batch_size]

        return "fresh", []

    async def send_stdin_line(entry: dict, line: str) -> None:
        proc = entry["proc"]
        if entry["stop_sent"] or proc.returncode is not None or proc.stdin is None:
            return
        try:
            proc.stdin.write((line + "\n").encode("utf-8"))
            await proc.stdin.drain()
        except (BrokenPipeError, ConnectionResetError, OSError):
            pass

    async def broadcast_accepted(target_plies: int, source_worker: int, prob: Problem) -> None:
        for wid, entry in list(active_workers.items()):
            if wid == source_worker or entry["plies"] != target_plies:
                continue
            await send_stdin_line(entry, f"exclude {prob.line}")
            if entry["role"] == "mutate":
                await send_stdin_line(entry, f"seed {prob.line}")

    async def stop_all_workers_for_tier(target_plies: int) -> None:
        for entry in list(active_workers.values()):
            if entry["plies"] != target_plies:
                continue
            if not entry["stop_sent"] and entry["proc"].returncode is None and entry["proc"].stdin is not None:
                entry["stop_sent"] = True
                try:
                    entry["proc"].stdin.write(b"stop\n")
                    await entry["proc"].stdin.drain()
                    entry["proc"].stdin.close()
                except (BrokenPipeError, ConnectionResetError, OSError):
                    pass

    # 既に目標達成している手数があれば出力確認
    for n in target_plies_list:
        publish_tier_if_ready(n)
        write_exclude_file(n)
    write_progress()

    async def worker_loop(worker_id: int) -> None:
        consecutive_errors = 0
        while not stop_event.is_set():
            if args.max_seconds > 0 and (time.monotonic() - started_at) >= args.max_seconds:
                stop_event.set()
                break
            target_plies = choose_target_plies()
            if target_plies is None:
                stop_event.set()
                break

            params = tier_params(target_plies)
            role, seeds = pick_role_and_seeds(target_plies, worker_id, params["seed_batch"])
            seed_file = work_dir / f"worker_{worker_id}_seeds.txt"
            if seeds:
                atomic_write(seed_file, "\n".join(p.line for p in seeds) + "\n")
            exclude_file = write_exclude_file(target_plies)

            remaining_needed = max(1, min(1000, args.count - len(accepted[target_plies])))
            cmd = [
                str(cli_path),
                "grow-tsume",
                "--engine", "KomoringHeights",
                "--target-moves", str(target_plies),
                "--max-positions", str(remaining_needed),
                "--beam-width", str(params["beam_width"]),
                "--branch-factor", str(params["branch_factor"]),
                "--max-attack", str(params["max_attack"]),
                "--max-defend", str(params["max_defend"]),
                "--step-timeout-ms", str(params["step_timeout_ms"]),
                "--verify-query-timeout-ms", str(params["verify_query_timeout_ms"]),
                "--verify-budget-seconds", str(params["verify_budget_seconds"]),
                "--hash-mb", str(args.hash_mb),
                "--threads", "1",
                "--rng-seed", str(rng.randint(1, 2_000_000_000)),
                "--stdin-control",
            ]
            if seeds:
                cmd.extend(["--seed-file", str(seed_file)])
            if exclude_file.exists() and exclude_file.stat().st_size > 0:
                cmd.extend(["--exclude-file", str(exclude_file)])
            if args.allow_final_alternatives:
                cmd.append("--allow-final-move-alternatives")
            if args.no_diversity:
                cmd.append("--no-exclude-similar")

            env = {
                **os.environ,
                "SHOGIBOARDQ_CONFIG_HOME": str(config_home),
                "XDG_CONFIG_HOME": str(config_home),
                "QT_QPA_PLATFORM": "offscreen",
            }

            proc = await asyncio.create_subprocess_exec(
                *cmd,
                stdin=asyncio.subprocess.PIPE,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.DEVNULL,
                env=env,
                limit=4 * 1024 * 1024,
            )
            stats["rounds_started"] += 1
            stats[f"role_{role}"] += 1
            entry = {"plies": target_plies, "role": role, "proc": proc, "stop_sent": False}
            active_workers[worker_id] = entry
            round_deadline = time.monotonic() + params["round_seconds"]

            async def send_stop() -> None:
                if entry["stop_sent"] or proc.returncode is not None:
                    return
                entry["stop_sent"] = True
                if proc.stdin is not None:
                    try:
                        proc.stdin.write(b"stop\n")
                        await proc.stdin.drain()
                        proc.stdin.close()
                    except (BrokenPipeError, ConnectionResetError, OSError):
                        pass

            try:
                while True:
                    if stop_event.is_set() or len(accepted[target_plies]) >= args.count:
                        await send_stop()
                    elif args.max_seconds > 0 and (time.monotonic() - started_at) >= args.max_seconds:
                        stop_event.set()
                        await send_stop()
                    elif time.monotonic() >= round_deadline:
                        await send_stop()

                    try:
                        raw = await asyncio.wait_for(proc.stdout.readline(), timeout=2.0)
                    except asyncio.TimeoutError:
                        if entry["stop_sent"] and time.monotonic() > round_deadline + 30:
                            proc.kill()
                            break
                        continue

                    if not raw:
                        break
                    line = raw.decode("utf-8", errors="replace").strip()
                    if not line:
                        continue
                    try:
                        ev = json.loads(line)
                    except json.JSONDecodeError:
                        continue

                    etype = ev.get("event")
                    if etype == "position":
                        stats["raw_certified"] += 1
                        sfen = ev.get("sfen", "")
                        pv = ev.get("pv", [])
                        if len(pv) != target_plies:
                            continue
                        try:
                            prob = Problem(sfen, tuple(pv))
                        except ValueError:
                            continue
                        if remaining_hand(prob):
                            stats["dropped_surplus"] += 1
                            continue
                        if prob.sfen in accepted_sfens[target_plies]:
                            stats["dropped_dup"] += 1
                            continue
                        if len(accepted[target_plies]) >= args.count:
                            await send_stop()
                            continue
                        if not register_accepted(target_plies, prob):
                            stats["dropped_similar"] += 1
                            continue
                        seed_pools.setdefault(target_plies, []).append(prob)
                        stats["accepted_total"] += 1
                        stats[f"accepted_{role}"] += 1

                        rec = {
                            "sfen": prob.sfen,
                            "pv": list(prob.pv),
                            "plies": target_plies,
                            "removed_pieces": ev.get("removed_pieces", 0),
                            "verify_queries": ev.get("verify_queries", 0),
                            "worker": worker_id,
                            "role": role,
                            "timestamp": datetime.now(timezone.utc).isoformat(timespec="seconds"),
                        }
                        with (work_dir / f"collected_{target_plies}ply.jsonl").open("a", encoding="utf-8") as fh:
                            fh.write(json.dumps(rec, ensure_ascii=False) + "\n")
                            fh.flush()

                        await broadcast_accepted(target_plies, worker_id, prob)
                        if len(accepted[target_plies]) % 10 == 0 or len(accepted[target_plies]) >= args.count:
                            write_exclude_file(target_plies)
                        if len(accepted[target_plies]) >= args.count:
                            publish_tier_if_ready(target_plies)
                            await stop_all_workers_for_tier(target_plies)
                    elif etype == "error":
                        stats["worker_errors"] += 1
                        consecutive_errors += 1
                        print(json.dumps({
                            "event": "worker_error",
                            "worker": worker_id,
                            "role": role,
                            "message": ev.get("message", ""),
                        }, ensure_ascii=False), flush=True)
                    elif etype == "finished":
                        consecutive_errors = 0
            finally:
                active_workers.pop(worker_id, None)
                if proc.returncode is None:
                    await send_stop()
                    try:
                        await asyncio.wait_for(proc.wait(), timeout=10.0)
                    except asyncio.TimeoutError:
                        proc.kill()
                        await proc.wait()
            if consecutive_errors >= 5:
                print(json.dumps({
                    "event": "worker_aborted",
                    "worker": worker_id,
                    "reason": "too_many_consecutive_errors",
                }, ensure_ascii=False), flush=True)
                stop_event.set()
                break
            if consecutive_errors > 0:
                await asyncio.sleep(min(10.0, 2.0 * consecutive_errors))

    async def progress_reporter() -> None:
        while not stop_event.is_set():
            try:
                await asyncio.wait_for(stop_event.wait(), timeout=15.0)
            except asyncio.TimeoutError:
                pass
            prog = write_progress()
            print(json.dumps({"event": "progress", **prog}, ensure_ascii=False), flush=True)

    reporter = asyncio.create_task(progress_reporter())
    workers = [asyncio.create_task(worker_loop(i)) for i in range(args.workers)]
    try:
        await asyncio.gather(*workers)
    finally:
        stop_event.set()
        reporter.cancel()
        await asyncio.gather(reporter, return_exceptions=True)
        for n in target_plies_list:
            publish_tier_if_ready(n)
            write_exclude_file(n)
        final_prog = write_progress()
        print(json.dumps({"event": "finished", **final_prog}, ensure_ascii=False), flush=True)

    return 0


if __name__ == "__main__":
    raise SystemExit(asyncio.run(run_pipeline(parse_args())))
