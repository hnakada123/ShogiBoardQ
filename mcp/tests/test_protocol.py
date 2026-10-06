"""Protocol negotiation: the newest MCP revision and the initialize handshake revisions."""

from __future__ import annotations

import sys

import pytest
from mcp_types.version import LATEST_HANDSHAKE_VERSION, LATEST_MODERN_VERSION

from conftest import PACKAGE_DIR

pytestmark = pytest.mark.anyio


async def _negotiate(env: dict[str, str], mode: str) -> tuple[str, int]:
    from mcp import Client, StdioServerParameters

    params = StdioServerParameters(command=sys.executable, args=["-m", "shogiboardq_mcp"], env=env, cwd=str(PACKAGE_DIR))
    async with Client(params, mode=mode) as client:
        tools = await client.list_tools()
        return client.protocol_version, len(tools.tools)


async def test_negotiates_latest_revision(server_env):
    """Clients that probe server/discover get the newest revision (2026-07-28 with mcp 2.x)."""
    version, tools = await _negotiate(server_env, "auto")
    assert version == LATEST_MODERN_VERSION
    assert tools > 40


async def test_initialize_handshake_still_works(server_env):
    """Clients that only know the initialize handshake (up to 2025-11-25) can still connect."""
    version, tools = await _negotiate(server_env, "legacy")
    assert version == LATEST_HANDSHAKE_VERSION
    assert tools > 40
