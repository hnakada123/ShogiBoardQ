"""Owned app processes must not survive a requested MCP shutdown."""
from __future__ import annotations

import asyncio
import subprocess
import sys

import pytest

from shogiboardq_mcp import app_client

pytestmark = pytest.mark.anyio


@pytest.mark.parametrize("ignore_termination", [False, True])
async def test_shutdown_reaps_unresponsive_owned_process(monkeypatch, ignore_termination):
    if ignore_termination and sys.platform.startswith("win"):
        pytest.skip("Windows terminate() cannot be ignored")
    monkeypatch.setenv("SHOGIBOARDQ_QUIT_APP_ON_EXIT", "1")
    monkeypatch.setattr(app_client, "EXIT_TIMEOUT", 0.1)
    code = "import signal,time; "
    if ignore_termination:
        code += "signal.signal(signal.SIGTERM, signal.SIG_IGN); "
    code += "print('ready', flush=True); time.sleep(60)"
    process = subprocess.Popen([sys.executable, "-c", code], stdout=subprocess.PIPE)
    client = app_client.AppClient()
    client._launched = process
    async def unexpected_connect():
        pytest.fail("Shutdown must not reconnect or launch another app")
    monkeypatch.setattr(client, "connect", unexpected_connect)
    try:
        assert await asyncio.to_thread(process.stdout.readline) == b"ready\n"
        await client.shutdown()
        assert process.poll() is not None
        assert client._launched is None
        await client.shutdown()  # repeat cleanup is harmless
    finally:
        if process.poll() is None:
            process.kill()
        process.wait(timeout=5)
        process.stdout.close()


async def test_shutdown_keeps_app_when_exit_not_requested(monkeypatch):
    monkeypatch.delenv("SHOGIBOARDQ_QUIT_APP_ON_EXIT", raising=False)
    process = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(60)"])
    client = app_client.AppClient()
    client._launched = process
    try:
        await client.shutdown()
        assert process.poll() is None
    finally:
        process.kill()
        process.wait(timeout=5)


async def test_shutdown_does_not_quit_connected_external_app(monkeypatch):
    monkeypatch.setenv("SHOGIBOARDQ_QUIT_APP_ON_EXIT", "1")
    client = app_client.AppClient()
    async def unexpected_quit():
        pytest.fail("An app the server did not launch must be preserved")
    monkeypatch.setattr(client, "_quit_connected", unexpected_quit)
    monkeypatch.setattr(client, "is_connected", lambda: True)
    await client.shutdown()
