#!/usr/bin/env python3
"""Linux: 実GUI終了後のエンジン残留を監視する。テストの子孫以外は停止しない。"""
import argparse
import ctypes
import json
import os
from pathlib import Path
import signal
import subprocess
import time


def proc_info(pid):
    try:
        stat = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()
        return {"pid": pid, "state": stat[0], "ppid": int(stat[1]),
                "ticks": int(stat[11]) + int(stat[12]), "start": stat[19],
                "exe": os.readlink(f"/proc/{pid}/exe") if stat[0] != "Z" else ""}
    except (FileNotFoundError, ProcessLookupError, PermissionError):
        return None


def children(pid):
    try:
        return [int(value) for value in Path(f"/proc/{pid}/task/{pid}/children").read_text().split()]
    except FileNotFoundError:
        return []


def run(args, mode, scenario, iteration):
    label = f"{mode}-{scenario}-{iteration}"
    env = dict(os.environ, QT_LOGGING_RULES="*.debug=false\n*.info=true\n*.warning=true\n*.critical=true",
               SHUTDOWN_ENGINE=str(args.engine.resolve()),
               SHUTDOWN_MODE=mode, SHUTDOWN_SCENARIO=scenario)
    env.setdefault("QT_QPA_PLATFORM", "offscreen")
    known = {}
    samples = {}
    started = time.monotonic()
    timeout = False
    resign_time = None
    post_resign = []
    with (args.output / f"{label}.log").open("w") as log:
        proc = subprocess.Popen([str(args.harness.resolve())], env=env,
                                stdout=log, stderr=log, start_new_session=True)
        try:
            while proc.poll() is None:
                pending = children(proc.pid)
                # subreaperに引き取られた子も追跡する。
                pending += [pid for pid in children(os.getpid()) if pid != proc.pid]
                while pending:
                    pid = pending.pop()
                    info = proc_info(pid)
                    if info:
                        known.setdefault(pid, info)
                        samples[pid] = dict(info, exe=info["exe"] or known[pid]["exe"])
                    pending.extend(children(pid))
                if "wait-" in scenario:
                    if resign_time is None and "RESIGN_COMPLETE" in (args.output / f"{label}.log").read_text():
                        resign_time = time.monotonic()
                    if resign_time is not None and len(post_resign) < 2:
                        delay = time.monotonic() - resign_time
                        if delay >= (0.2 if not post_resign else 1.2):
                            post_resign.append({"seconds": round(delay, 3), "processes": [
                                info for pid in known if (info := proc_info(pid)) and info["state"] != "Z"]})
                if time.monotonic() - started > 40:
                    timeout = True
                    break
                time.sleep(0.01)
            exit_time = time.monotonic() - started
            # 終了直後と猶予後を分けて記録し、CPU時間の増加も検査する。
            at_exit = [info for pid in known if (info := proc_info(pid)) and info["state"] != "Z"]
            time.sleep(0.5)
            remaining = [info for pid in known if (info := proc_info(pid)) and info["state"] != "Z"]
            result = {"case": label, "returncode": proc.poll(), "timeout": timeout,
                      "seconds": round(exit_time, 3), "processes": list(samples.values()),
                      "alive_at_exit": at_exit, "alive_after_500ms": remaining,
                      "post_resign": post_resign}
        finally:
            # 正常終了の観測を終えてから、異常時だけ今回のプロセス群を回収する。
            try:
                os.killpg(proc.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            proc.wait()
            for pid, original in known.items():
                info = proc_info(pid)
                if info and info["start"] == original["start"]:
                    try:
                        os.kill(pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    try:
                        os.waitpid(pid, 0)
                    except ChildProcessError:
                        pass
    log_text = (args.output / f"{label}.log").read_text()
    result["closed"] = "CLOSE_REQUEST" in log_text and "MAIN_WINDOW_DESTROYED" in log_text
    result["post_resign_reaped"] = ("wait-" not in scenario
        or (len(post_resign) == 2 and not post_resign[-1]["processes"]))
    result["passed"] = (result["returncode"] == 0 and result["closed"]
                        and not remaining and bool(known) and result["post_resign_reaped"])
    print(json.dumps(result, ensure_ascii=False), flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--engine", type=Path, required=True)
    parser.add_argument("--harness", type=Path, default=Path("build/gui-audit/test-build/engine_shutdown_harness"))
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--modes", nargs="+", default=["human-black", "human-white", "eve"])
    parser.add_argument("--scenarios", nargs="+", default=["thinking-close", "thinking-menu",
        "ponder-close", "ponder-menu", "startup-close", "startup-menu", "break-close",
        "break-restart-close", "resign-close", "cancel-close"])
    parser.add_argument("--repeat", type=int, default=1)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    # 孤児になったテストエンジンを自分の子として回収し、ゾンビを残さない。
    if ctypes.CDLL(None, use_errno=True).prctl(36, 1, 0, 0, 0) != 0:  # PR_SET_CHILD_SUBREAPER
        raise OSError(ctypes.get_errno(), "prctl(PR_SET_CHILD_SUBREAPER)")
    results = [run(args, mode, scenario, i + 1) for mode in args.modes
               for scenario in args.scenarios for i in range(args.repeat)
               if not (mode == "eve" and scenario == "resign-close")]
    (args.output / "results.json").write_text(json.dumps(results, ensure_ascii=False, indent=2) + "\n")
    return 0 if all(result["passed"] for result in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
