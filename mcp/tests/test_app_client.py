"""Owned app processes must not survive a requested MCP shutdown."""
from __future__ import annotations

import asyncio
import subprocess
import sys

import pytest

from conftest import write_python_program
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
        # Windows text-mode stdout ends the line with \r\n
        assert (await asyncio.to_thread(process.stdout.readline)).rstrip(b"\r\n") == b"ready"
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


def _fake_queued_app(monkeypatch, client, states):
    """Answer a queued UI call, then each ui.interaction poll from ``states``."""
    polls = iter(states)

    async def fake_call(method, params=None, timeout=app_client.CALL_TIMEOUT):
        if method == "widget.click":
            return {"queued": True, "interaction": 7, "object_name": "button"}
        assert method == "ui.interaction" and params == {"id": 7}
        return next(polls)

    monkeypatch.setattr(client, "call", fake_call)


async def test_call_queued_waits_until_the_operation_ran(monkeypatch):
    client = app_client.AppClient()
    _fake_queued_app(monkeypatch, client, [{"interaction": 7, "state": "pending"},
                                           {"interaction": 7, "state": "done"}])
    result = await client.call_queued("widget.click", {"widget": "button"})
    assert result == {"queued": True, "interaction": 7, "object_name": "button", "state": "done"}


async def test_call_queued_returns_an_operation_that_opened_a_modal_dialog(monkeypatch):
    client = app_client.AppClient()
    _fake_queued_app(monkeypatch, client, [{"interaction": 7, "state": "running"}])
    assert (await client.call_queued("widget.click"))["state"] == "running"


@pytest.mark.parametrize("state", ["failed", "cancelled"])
async def test_call_queued_raises_a_failure_at_run_time(monkeypatch, state):
    # Runtime failures are not swallowed: the application's error kind reaches the MCP client.
    client = app_client.AppClient()
    error = {"code": -32002, "message": "The target is hidden, disabled or blocked by another dialog",
             "data": {"slug": "invalid_state"}}
    _fake_queued_app(monkeypatch, client, [{"interaction": 7, "state": state, "error": error}])
    with pytest.raises(app_client.AppError) as raised:
        await client.call_queued("widget.click")
    assert raised.value.code == "invalid_state"
    assert raised.value.rpc_code == -32002
    assert "blocked" in raised.value.message


def test_windows_pipe_address_has_the_pipe_prefix():
    # A bare QLocalServer name would open a file in the current directory instead of the named pipe.
    assert app_client.AppClient.pipe_address("shogiboardq-automation-user") == "\\\\.\\pipe\\shogiboardq-automation-user"
    assert app_client.AppClient.pipe_address("\\\\.\\pipe\\custom") == "\\\\.\\pipe\\custom"


@pytest.mark.skipif(sys.platform.startswith("win"), reason="Windows uses named pipes")
async def test_launch_rejects_a_socket_path_the_app_cannot_listen_on(monkeypatch, tmp_path, cli_path):
    # An over-long path used to cost LAUNCH_TIMEOUT and leave the app running without the API.
    # The limit is the application's (shogiboardq-cli environment), not one copied into Python.
    monkeypatch.setenv("SHOGIBOARDQ_CLI", str(cli_path))
    monkeypatch.setenv("SHOGIBOARDQ_EXECUTABLE", sys.executable)
    limit = app_client.environment()["max_socket_path_bytes"]
    long_path = "/" + "x" * limit + "/app.sock"
    monkeypatch.setenv("SHOGIBOARDQ_AUTOMATION_SOCKET", long_path)
    real_popen = subprocess.Popen
    def no_app_launch(args, *rest, **kwargs):
        # shogiboardq-cli environment may run (the socket variable changed); the app must not.
        if isinstance(args, list) and "--automation" in args:
            pytest.fail("The app must not be launched on a socket path it cannot listen on")
        return real_popen(args, *rest, **kwargs)
    monkeypatch.setattr(app_client.subprocess, "Popen", no_app_launch)
    with pytest.raises(app_client.ToolError) as raised:
        await app_client.AppClient()._launch_and_connect([])
    assert raised.value.code == "app_unavailable"
    assert f"at most {limit}" in raised.value.message
    fits = "/" + "x" * (limit - 1 - len("/app.sock")) + "/app.sock"
    assert len(fits) == limit
    assert app_client.socket_path_problem(fits) is None
    assert app_client.socket_path_problem(fits + "x") is not None


async def test_launch_timeout_stops_the_launched_app(monkeypatch, tmp_path):
    # An app that never opens the socket is unreachable; it must not keep running after the error.
    fake = write_python_program(tmp_path / "fake-app", "import time\ntime.sleep(60)\n")
    monkeypatch.setenv("SHOGIBOARDQ_EXECUTABLE", str(fake))
    monkeypatch.setenv("SHOGIBOARDQ_AUTOMATION_SOCKET", str(tmp_path / "app.sock"))
    monkeypatch.setattr(app_client, "LAUNCH_TIMEOUT", 0.5)
    monkeypatch.setattr(app_client, "EXIT_TIMEOUT", 1.0)
    launched: list[subprocess.Popen] = []
    real_popen = subprocess.Popen
    def recording_popen(args, *rest, **kwargs):
        # Only the app; the socket path check may also run shogiboardq-cli environment.
        process = real_popen(args, *rest, **kwargs)
        if isinstance(args, list) and args and args[0] == str(fake):
            launched.append(process)
        return process
    monkeypatch.setattr(app_client.subprocess, "Popen", recording_popen)
    client = app_client.AppClient()
    try:
        with pytest.raises(app_client.ToolError) as raised:
            await client._launch_and_connect([])
        assert raised.value.code == "app_unavailable"
        assert len(launched) == 1
        assert launched[0].poll() is not None
        assert client._launched is None
    finally:
        for process in launched:
            if process.poll() is None:
                process.kill()
            process.wait(timeout=5)
