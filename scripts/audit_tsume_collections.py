#!/usr/bin/env python3
"""実エンジンによる不要駒除去を並列実行し、再開可能な監査記録と重複除外済み結果を保存する。"""
import argparse
import asyncio
from collections import Counter, defaultdict
import hashlib
import json
from pathlib import Path


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="+", type=Path)
    parser.add_argument("--auditor", type=Path, default=Path("build/tests/tsumeshogi_collection_auditor"))
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--workers", type=int, default=8)
    parser.add_argument("--timeout-ms", type=int, default=15000)
    return parser.parse_args()


async def run(args):
    if args.workers < 1 or args.timeout_ms < 1:
        raise ValueError("workers と timeout-ms は正数が必要です")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    signature = hashlib.sha256(args.auditor.read_bytes()).hexdigest()
    log_path = args.output_dir / "audit.jsonl"
    records = {}
    if log_path.exists():
        for line in log_path.read_text().splitlines():
            record = json.loads(line)
            if record["auditor_sha256"] != signature:
                raise ValueError("検証器が変わっています。新しい出力ディレクトリを指定してください")
            if record["status"] != "unknown" or record["timeout_ms"] >= args.timeout_ms:
                records[(record["original"], record["target"])] = record
    source_keys = []
    for path in args.inputs:
        for number, line in enumerate(path.read_text().splitlines(), 1):
            if not line.strip() or line.startswith("#"):
                continue
            sfen, moves = line.split(" moves ", 1)
            source_keys.append((sfen, len(moves.split())))
    queue = asyncio.Queue()
    for key in dict.fromkeys(source_keys):
        if key not in records:
            queue.put_nowait(key)
    initial = queue.qsize()
    log = log_path.open("a", encoding="utf-8")

    def checkpoint():
        grouped = defaultdict(dict)
        counts = Counter()
        changes = Counter()
        for key in source_keys:
            record = records.get(key)
            if record is None:
                continue
            counts[(key[1], record["status"])] += 1
            if record["status"] == "minimal":
                grouped[key[1]][record["sfen"]] = record["sfen"] + " moves " + " ".join(record["pv"])
                changes[key[1]] += record["removed"] > 0
        summary = {str(n): {"input": sum(k[1] == n for k in source_keys),
                           "unique_minimal": len(grouped[n]), "changed": changes[n],
                           "statuses": {status: count for (plies, status), count in counts.items() if plies == n}}
                   for n in sorted({key[1] for key in source_keys})}
        for n, positions in grouped.items():
            path = args.output_dir / f"tsume_{n}ply_minimal.txt"
            tmp = path.with_suffix(".tmp")
            tmp.write_text("\n".join(positions.values()) + "\n")
            tmp.replace(path)
        path = args.output_dir / "summary.json"
        temporary = path.with_suffix(".tmp")
        temporary.write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n")
        temporary.replace(path)
        return summary

    async def worker(number):
        with (args.output_dir / f"worker_{number}.stderr.log").open("ab") as err:
            process = await asyncio.create_subprocess_exec(str(args.auditor.resolve()), str(args.engine.resolve()),
                stdin=asyncio.subprocess.PIPE, stdout=asyncio.subprocess.PIPE, stderr=err,
                limit=1024 * 1024)
            try:
                while not queue.empty():
                    key = queue.get_nowait()
                    request = {"sfen": key[0], "target": key[1], "timeout_ms": args.timeout_ms}
                    process.stdin.write((json.dumps(request) + "\n").encode())
                    await process.stdin.drain()
                    raw = await asyncio.wait_for(process.stdout.readline(), args.timeout_ms / 1000 * 100 + 60)
                    if not raw:
                        raise RuntimeError(f"検証器 {number} が応答せず終了しました")
                    record = json.loads(raw)
                    assert (record["original"], record["target"]) == key
                    record.update(auditor_sha256=signature, timeout_ms=args.timeout_ms)
                    records[key] = record
                    log.write(json.dumps(record, ensure_ascii=False) + "\n")
                    log.flush()
                    queue.task_done()
            finally:
                process.stdin.close()
                try:
                    await asyncio.wait_for(process.wait(), 5)
                except asyncio.TimeoutError:
                    process.kill()
                    await process.wait()

    async def progress():
        while True:
            await asyncio.sleep(15)
            print(json.dumps({"remaining": queue.qsize(), "initial": initial, "summary": checkpoint()}), flush=True)

    reporter = asyncio.create_task(progress())
    workers = [asyncio.create_task(worker(i)) for i in range(args.workers)]
    try:
        await asyncio.gather(*workers)
    finally:
        reporter.cancel()
        for task in workers:
            task.cancel()
        await asyncio.gather(reporter, *workers, return_exceptions=True)
        log.close()
        print(json.dumps(checkpoint(), ensure_ascii=False, indent=2), flush=True)


if __name__ == "__main__":
    asyncio.run(run(arguments()))
