#!/usr/bin/env python3
"""全ワーカー共通の類似判定と実エンジン監査を通して問題集を補充する。

元ファイルは変更しない。出力は再開可能な作業用問題集・監査記録・不足数。
完成後も audit_tsume_collections.py による独立した最終監査を行うこと。
"""
import argparse
import asyncio
from collections import Counter
import hashlib
import json
from pathlib import Path
import time

from tsume_diversity import DiversityIndex, Problem, board_from_sfen


def atomic(path, text):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(text, encoding="utf-8")
    temporary.replace(path)


def seed_sample(group, maximum, offset):
    """直近半分を優先し、残りは全履歴から巡回抽出する。新しい系統を即座に探索へ渡す。"""
    if len(group) <= maximum:
        return list(group)
    recent_count = maximum // 2
    recent = group[-recent_count:]
    older = group[:-recent_count]
    count = maximum - recent_count
    return [older[(offset + i * len(older) // count) % len(older)] for i in range(count)] + recent


def arguments():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("inputs", nargs="+", type=Path)
    p.add_argument("--engine", required=True, type=Path)
    p.add_argument("--output-dir", required=True, type=Path)
    p.add_argument("--sampler", type=Path, default=Path("build/tests/tsumeshogi_diversity_sampler"))
    p.add_argument("--auditor", type=Path, default=Path("build/tests/tsumeshogi_collection_auditor"))
    p.add_argument("--workers", type=int, default=10)
    p.add_argument("--count", type=int, default=1000)
    p.add_argument("--seconds", type=int, default=3600)
    p.add_argument("--timeout-ms", type=int, default=15000)
    p.add_argument("--screen-ms", type=int, default=5)
    p.add_argument("--exclude-sfens", type=Path,
                   help="最終監査等で除外するSFENの一覧。除外は作業ディレクトリに保存される")
    return p.parse_args()


async def run(args):
    if min(args.workers, args.count, args.seconds, args.timeout_ms, args.screen_ms) < 1:
        raise ValueError("件数・時間・ワーカー数は正数が必要です")
    root = args.output_dir
    root.mkdir(parents=True, exist_ok=True)
    exclusions_path = root / "exclusions.json"
    exclusions = set(json.loads(exclusions_path.read_text())) if exclusions_path.exists() else set()
    if args.exclude_sfens:
        for line in args.exclude_sfens.read_text().splitlines():
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            sfen = " ".join(line.split(" moves ", 1)[0].split())
            board_from_sfen(sfen)
            exclusions.add(sfen)
    binary_hash = hashlib.sha256(args.auditor.read_bytes()).hexdigest()
    indexes = {}
    accepted_sfens = {}
    initial = {}
    inputs = {}
    seen = set()
    counters = Counter()
    log_path = root / "audit.jsonl"
    rejected_path = root / "checked.jsonl"
    started = time.monotonic()
    children = set()
    exploration = {}
    frontier = {}

    def accept(problem):
        if problem.sfen in exclusions:
            return False
        n = len(problem.pv)
        index = indexes.setdefault(n, DiversityIndex())
        sfens = accepted_sfens.setdefault(n, set())
        if problem.sfen in sfens or not index.add(problem):
            return False
        sfens.add(problem.sfen)
        return True

    def capacity(n):
        # 長手数のための異なる種を、完成済みの9・11手詰にも追加収集できるようにする。
        # 公開用ファイルには先頭count題だけを保存する。
        need_longer = any(k > n and len(index.problems) < args.count for k, index in indexes.items())
        return args.count * 3 if n >= 9 and need_longer else args.count

    def remember_neighbour(problem):
        index = indexes.get(len(problem.pv))
        if index is None:
            return
        family = index.conflict(problem)
        if family is None:
            return
        geometry = problem.geometry
        if geometry == index.problems[family].geometry:
            return
        group = exploration.setdefault((len(problem.pv), family), {})
        if geometry not in group:
            group[geometry] = problem
            if len(group) > 3:
                del group[next(iter(group))]

    for path in args.inputs:
        content = path.read_text(encoding="utf-8")
        inputs[str(path)] = hashlib.sha256(path.read_bytes()).hexdigest()
        for line in content.splitlines():
            if not line.strip() or line.startswith("#"):
                continue
            problem = Problem.parse(line)
            n = len(problem.pv)
            index = indexes.setdefault(n, DiversityIndex())
            if len(index.problems) < args.count:
                accept(problem)
            seen.add((n, problem.sfen))
    seen.update((n, sfen) for n in indexes for sfen in exclusions)
    for n, index in indexes.items():
        initial[n] = len(index.problems)

    manifest = {"inputs": inputs, "auditor_sha256": binary_hash, "count": args.count,
                "diversity_sha256": hashlib.sha256(Path(__file__).with_name("tsume_diversity.py").read_bytes()).hexdigest()}
    manifest_path = root / "run.json"
    if manifest_path.exists() and json.loads(manifest_path.read_text()) != manifest:
        raise ValueError("入力または検査器が変わっています。新しい出力先が必要です")
    atomic(manifest_path, json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
    atomic(exclusions_path, json.dumps(sorted(exclusions), ensure_ascii=False, indent=2) + "\n")
    if log_path.exists():
        for line in log_path.read_text().splitlines():
            record = json.loads(line)
            if record["auditor_sha256"] != binary_hash:
                raise ValueError("監査ログと現在の検査器が一致しません")
            seen.add((record["target"], record["original"]))
            if record["status"] == "minimal":
                problem = Problem(record["sfen"], tuple(record["pv"]))
                index = indexes.get(record["target"])
                if index is not None and len(index.problems) < capacity(record["target"]):
                    accept(problem)
    if rejected_path.exists():
        for line in rejected_path.read_text().splitlines():
            n, sfen = json.loads(line)
            seen.add((n, sfen))
    exploration_path = root / "exploration.txt"
    if exploration_path.exists():
        for line in exploration_path.read_text().splitlines():
            if line:
                remember_neighbour(Problem.parse(line))
    frontier_path = root / "frontier.txt"
    if frontier_path.exists():
        for line in frontier_path.read_text().splitlines():
            if line:
                problem = Problem.parse(line)
                frontier.setdefault(len(problem.pv), {})[problem.geometry] = problem

    def complete():
        return all(len(i.problems) >= args.count for i in indexes.values())

    def checkpoint():
        summary = {"elapsed_seconds": round(time.monotonic() - started), "counters": dict(counters),
                   "excluded_positions": len(exclusions), "files": {}}
        atomic(root / "known_sfens.txt", "\n".join(sorted({sfen for _, sfen in seen})) + "\n")
        for n, index in sorted(indexes.items()):
            published = index.problems[:args.count]
            atomic(root / f"tsume_{n}ply_working.txt", "\n".join(p.line for p in published) + "\n")
            summary["files"][str(n)] = {"initial_representatives": initial[n], "current": len(published),
                                         "verified_seed_positions": len(index.problems),
                                         "missing": max(0, args.count - len(index.problems))}
        # 短手数の大量の種で長手数の種を押し流さない。系統ごとに1題で、手数別に最大200題。
        pending = [n for n, index in sorted(indexes.items()) if len(index.problems) < args.count]
        neighbours = [p for group in exploration.values() for p in group.values()]
        atomic(exploration_path, "\n".join(p.line for p in neighbours) + "\n")
        atomic(frontier_path, "\n".join(p.line for group in frontier.values() for p in group.values()) + "\n")
        for worker_number in range(args.workers):
            depth = pending[worker_number % len(pending)] if pending else max(indexes)
            # 終盤は種を増やす担当を減らし、不足している手数そのものの補充へ集中する。
            source_interval = 8 if len(indexes[depth].problems) * 4 >= args.count * 3 else 4
            if len(pending) == 1 and worker_number % source_interval == 0 and depth - 2 in indexes:
                lower = depth - 2
                if args.count <= len(indexes[lower].problems) < capacity(lower):
                    depth = lower
            seeds = []
            certified = []
            for n, index in sorted(indexes.items()):
                if max(3, depth - 4) <= n <= depth:
                    group = index.problems
                    offset = int(time.monotonic() - started) // 15 * 137 + worker_number * 29
                    representatives = seed_sample(group, 200, offset)
                    seeds.extend(representatives)
                    certified.extend(representatives)
                    nearby = [p for p in neighbours if len(p.pv) == n]
                    if nearby:
                        seeds.extend(seed_sample(nearby, 100, offset))
                    novel = list(frontier.get(n, {}).values())
                    if novel:
                        seeds.extend(seed_sample(novel, 50, offset))
            atomic(root / f"seeds_worker_{worker_number}.txt", "\n".join(p.line for p in seeds) + "\n")
            atomic(root / f"seeds_worker_{worker_number}.txt.certified", "\n".join(p.line for p in certified) + "\n")
            active = [n for n, index in indexes.items() if len(index.problems) < capacity(n)]
            atomic(root / f"seeds_worker_{worker_number}.txt.minimum", str(min(active, default=max(indexes))))
            atomic(root / f"seeds_worker_{worker_number}.txt.target", str(max(3, depth)))
        atomic(root / "progress.json", json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        return summary

    async def finish(process):
        if process.returncode is None:
            if process.stdin is not None:
                process.stdin.close()  # 監査器の終了処理でUSIエンジンもquitする。
            else:
                process.terminate()
        try:
            await asyncio.wait_for(process.wait(), 5)
        except asyncio.TimeoutError:
            process.kill()
            await process.wait()
        children.discard(process)

    async def worker(number, audit_log, checked_log):
        depth = max(indexes)
        with (root / f"sampler_{number}.log").open("ab") as sampler_err, (root / f"auditor_{number}.log").open("ab") as auditor_err:
            auditor = await asyncio.create_subprocess_exec(str(args.auditor.resolve()), str(args.engine.resolve()),
                stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE, stderr=auditor_err, limit=1024 * 1024)
            children.add(auditor)
            sampler = await asyncio.create_subprocess_exec(str(args.sampler.resolve()), str(root / f"seeds_worker_{number}.txt"),
                str(depth), str(args.screen_ms * (1, 2, 5, 10)[number % 4]), str(number), str(args.seconds),
                stdout=asyncio.subprocess.PIPE, stderr=sampler_err)
            children.add(sampler)
            remover = await asyncio.create_subprocess_exec(str(args.sampler.resolve()), "--removals",
                stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE, stderr=sampler_err)
            children.add(remover)

            async def audit_candidate(sfen, target, source):
                request = {"sfen": sfen, "target": target, "timeout_ms": args.timeout_ms}
                auditor.stdin.write((json.dumps(request) + "\n").encode())
                await auditor.stdin.drain()
                reply = await asyncio.wait_for(auditor.stdout.readline(), args.timeout_ms / 1000 * 100 + 60)
                if not reply:
                    raise RuntimeError(f"監査器 {number} が終了しました")
                record = json.loads(reply)
                if (record["original"], record["target"]) != (sfen, target):
                    raise RuntimeError("監査の要求と応答が一致しません")
                record.update(auditor_sha256=binary_hash, timeout_ms=args.timeout_ms, source=source, worker=number)
                audit_log.write(json.dumps(record) + "\n")
                audit_log.flush()
                counters[record["status"]] += 1
                if record["status"] == "minimal":
                    clean = Problem(record["sfen"], tuple(record["pv"]))
                    # await後は、他ワーカーの採択と不要駒除去後の手順をもう一度確認する。
                    index = indexes[target]
                    if len(index.problems) < capacity(target) and accept(clean):
                        kind = "accepted" if len(index.problems) <= args.count else "extra_seed"
                        counters[f"{kind}_{target}"] += 1
                        counters[f"{kind}_{target}_worker_{number}"] += 1
                    else:
                        counters["similar_after_audit"] += 1
                return record

            try:
                async for raw in sampler.stdout:
                    if complete():
                        break
                    event = json.loads(raw)
                    n = event["target"]
                    index = indexes.get(n)
                    counters["candidates"] += 1
                    if index is None or len(index.problems) >= capacity(n):
                        continue
                    problem = Problem(event["sfen"], tuple(event["pv"]))
                    key = (n, problem.sfen)
                    if key in seen:
                        counters["known_sfen"] += 1
                        continue
                    seen.add(key)
                    if index.conflict(problem) is not None:
                        # 採択はしないが、手順が変化した近傍を各系統3題まで探索用に残す。
                        # 次の変更で違う詰め筋へ到達するための中間局面であり、問題数には含めない。
                        remember_neighbour(problem)
                        counters["similar_before_audit"] += 1
                        checked_log.write(json.dumps(key) + "\n")
                        checked_log.flush()
                        continue
                    record = await audit_candidate(problem.sfen, n, event["source"])
                    if record["status"] == "multiple":
                        # 新しい手順で余詰のある候補は修正探索用だけに使う。
                        # 採択数には加えず、各手数300系統で打ち切る。
                        group = frontier.setdefault(n, {})
                        group[problem.geometry] = problem
                        if len(group) > 300:
                            del group[next(iter(group))]
                        remover.stdin.write((json.dumps({"sfen": problem.sfen}) + "\n").encode())
                        await remover.stdin.drain()
                        removed = await asyncio.wait_for(remover.stdout.readline(), 10)
                        if not removed:
                            raise RuntimeError("駒除去候補を取得できません")
                        candidates = json.loads(removed)
                        if not isinstance(candidates, list):
                            raise RuntimeError("駒除去候補の応答が不正です")
                        # 検証は省略しない。余詰のある元局面を採択することもない。
                        # 各候補につき最大2通り。残りは近傍探索に任せる。
                        candidates = sorted(candidates, key=lambda s: hashlib.sha256(s.encode()).digest())[:2]
                        for sfen in candidates:
                            if len(index.problems) >= capacity(n):
                                break
                            if (n, sfen) in seen:
                                continue
                            seen.add((n, sfen))
                            counters["repair_attempts"] += 1
                            await audit_candidate(sfen, n, "repair:" + event["source"])
                if not complete() and await sampler.wait() != 0:
                    raise RuntimeError(f"候補生成器 {number} が異常終了しました")
            finally:
                await finish(sampler)
                await finish(auditor)
                await finish(remover)

    async def progress():
        while True:
            await asyncio.sleep(15)
            print(json.dumps(checkpoint(), ensure_ascii=False), flush=True)

    checkpoint()
    if complete():
        return
    with log_path.open("a") as audit_log, rejected_path.open("a") as checked_log:
        reporter = asyncio.create_task(progress())
        workers = [asyncio.create_task(worker(n, audit_log, checked_log)) for n in range(args.workers)]
        try:
            await asyncio.gather(*workers)
        finally:
            reporter.cancel()
            for task in workers:
                task.cancel()
            await asyncio.gather(reporter, *workers, return_exceptions=True)
            for process in list(children):
                await finish(process)
            print(json.dumps(checkpoint(), ensure_ascii=False, indent=2), flush=True)


if __name__ == "__main__":
    asyncio.run(run(arguments()))
