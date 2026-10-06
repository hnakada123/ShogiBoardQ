"""Entry point: ``python -m shogiboardq_mcp`` starts the stdio MCP server."""

from __future__ import annotations

import asyncio
import importlib.metadata
import logging
import os
import sys


def _mcp_major_version() -> int | None:
    try:
        return int(importlib.metadata.version("mcp").split(".")[0])
    except (importlib.metadata.PackageNotFoundError, ValueError):
        return None


def main() -> int:
    level = logging.DEBUG if os.environ.get("SHOGIBOARDQ_MCP_DEBUG") else logging.WARNING
    # stdout is reserved for the MCP transport; all logging goes to stderr.
    logging.basicConfig(level=level, stream=sys.stderr, format="%(levelname)s %(name)s: %(message)s")
    sdk = _mcp_major_version()
    if sdk is not None and sdk < 2:
        print("shogiboardq-mcp requires the mcp package 2.0 or later (installed: "
              f"{importlib.metadata.version('mcp')}). Install it with: pip install 'mcp>=2,<3'", file=sys.stderr)
        return 1
    from .server import run

    try:
        asyncio.run(run())
    except KeyboardInterrupt:
        return 130
    return 0


if __name__ == "__main__":
    sys.exit(main())
