"""Entry point: ``python -m shogiboardq_mcp`` starts the stdio MCP server."""

from __future__ import annotations

import asyncio
import logging
import os
import sys


def main() -> int:
    level = logging.DEBUG if os.environ.get("SHOGIBOARDQ_MCP_DEBUG") else logging.WARNING
    # stdout is reserved for the MCP transport; all logging goes to stderr.
    logging.basicConfig(level=level, stream=sys.stderr, format="%(levelname)s %(name)s: %(message)s")
    from .server import run

    try:
        asyncio.run(run())
    except KeyboardInterrupt:
        return 130
    return 0


if __name__ == "__main__":
    sys.exit(main())
