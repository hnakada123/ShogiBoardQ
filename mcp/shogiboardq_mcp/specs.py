"""Parameter tables of shogiboardq-cli and the automation API, as JSON Schema.

``specs.json`` is ``shogiboardq-cli describe`` (the C++ tables CliCommandSpecs and AutomationMethodSpecs)
saved with the package, so the server builds the input schemas of the tools that pass their arguments on
from the C++ tables instead of copying them. Regenerate it after changing a table::

    python3 -m shogiboardq_mcp.specs /path/to/shogiboardq-cli

``mcp/tests/test_param_specs.py`` fails when the file is out of date.
"""

from __future__ import annotations

import copy
import json
import os
import subprocess
import sys
from functools import lru_cache
from pathlib import Path
from typing import Any, Iterable

SPECS_PATH = Path(__file__).with_name("specs.json")


@lru_cache(maxsize=1)
def _load() -> dict[str, dict[str, dict[str, Any]]]:
    return json.loads(SPECS_PATH.read_text(encoding="utf-8"))


def operation(kind: str, name: str) -> dict[str, Any]:
    """Input schema of a CLI command (kind "command") or automation method (kind "method")."""
    return copy.deepcopy(_load()[kind][name])


def schema(
    kind: str,
    name: str,
    *,
    drop: Iterable[str] = (),
    override: dict[str, dict[str, Any]] | None = None,
    extra: dict[str, dict[str, Any]] | None = None,
) -> dict[str, Any]:
    """Input schema of an MCP tool built on an operation.

    ``drop`` removes parameters the tool does not expose (``stdin_control``, which the job manager sets),
    ``override`` changes attributes for the tool (a narrower range, a description that mentions MCP paths),
    ``extra`` adds parameters the server handles itself (they are not passed on).
    """
    result = operation(kind, name)
    properties = result["properties"]
    dropped = set(drop)
    for param in dropped:
        del properties[param]
    for param, attributes in (override or {}).items():
        properties[param] = {**properties[param], **attributes}
    for param, prop in (extra or {}).items():
        if param in properties:
            raise ValueError(f"{name}.{param} is already a parameter of the operation")
        properties[param] = prop
    required = [p for p in result.get("required", []) if p not in dropped]
    if required:
        result["required"] = required
    else:
        result.pop("required", None)
    return result


def passed_on(kind: str, name: str, args: dict[str, Any]) -> dict[str, Any]:
    """The arguments of a tool call that belong to the operation (MCP-only parameters removed)."""
    known = _load()[kind][name]["properties"]
    return {key: value for key, value in args.items() if key in known}


def generate(cli: str) -> dict[str, dict[str, dict[str, Any]]]:
    env = dict(os.environ, QT_QPA_PLATFORM="offscreen")
    out = subprocess.run([cli, "describe"], capture_output=True, text=True, timeout=60, env=env, check=True)
    data = json.loads(out.stdout)
    if not data.get("ok"):
        raise RuntimeError("shogiboardq-cli describe failed")
    return {
        "command": {c["name"]: c["input_schema"] for c in data["commands"]},
        "method": {m["name"]: m["input_schema"] for m in data["methods"]},
    }


def dumps(specs: dict[str, Any]) -> str:
    return json.dumps(specs, ensure_ascii=False, indent=1, sort_keys=True) + "\n"


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("usage: python3 -m shogiboardq_mcp.specs /path/to/shogiboardq-cli", file=sys.stderr)
        return 2
    SPECS_PATH.write_text(dumps(generate(argv[1])), encoding="utf-8")
    print(f"wrote {SPECS_PATH}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
