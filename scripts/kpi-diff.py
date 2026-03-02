#!/usr/bin/env python3
"""Compare two KPI JSON files and output a Markdown diff table.

Usage: kpi-diff.py <base-kpi.json> <pr-kpi.json>
Output: Markdown table to stdout (for GitHub Step Summary)
"""

import json
import sys


def load_kpi(path):
    with open(path) as f:
        return json.load(f)


def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <base-kpi.json> <pr-kpi.json>", file=sys.stderr)
        sys.exit(1)

    base = load_kpi(sys.argv[1])
    pr = load_kpi(sys.argv[2])

    all_keys = sorted(set(list(base.keys()) + list(pr.keys())))

    if not all_keys:
        print("No KPI data to compare.")
        return

    print("### KPI Diff (base \u2192 PR)")
    print("")
    print("| Metric | Base | PR | Delta | Limit | Status |")
    print("|--------|-----:|---:|------:|------:|--------|")

    has_regression = False

    for key in all_keys:
        b = base.get(key, {})
        p = pr.get(key, {})

        base_val = b.get("value") if b else None
        pr_val = p.get("value") if p else None
        limit = p.get("limit") or b.get("limit")

        base_str = str(base_val) if base_val is not None else "\u2014"
        pr_str = str(pr_val) if pr_val is not None else "\u2014"
        limit_str = str(limit) if limit is not None else "\u2014"

        # Delta
        if base_val is not None and pr_val is not None:
            delta = pr_val - base_val
            if delta > 0:
                delta_str = f"+{delta}"
            elif delta < 0:
                delta_str = str(delta)
            else:
                delta_str = "0"
        elif base_val is None:
            delta_str = "new"
        elif pr_val is None:
            delta_str = "removed"
        else:
            delta_str = "\u2014"

        # Status
        if base_val is not None and pr_val is not None:
            delta = pr_val - base_val
            if delta > 0:
                if limit is not None and pr_val > limit:
                    status = ":x: **exceeded limit**"
                else:
                    status = ":warning: regression"
                has_regression = True
            elif delta < 0:
                status = ":white_check_mark: improved"
            else:
                status = "\u2014"
        elif base_val is None:
            status = ":new: new metric"
        elif pr_val is None:
            status = "removed"
        else:
            status = "\u2014"

        print(f"| {key} | {base_str} | {pr_str} | {delta_str} | {limit_str} | {status} |")

    print("")
    if has_regression:
        print("> :warning: KPI regressions detected. Please review before merging.")
    else:
        print("> :white_check_mark: No KPI regressions.")


if __name__ == "__main__":
    main()
