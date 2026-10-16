"""Error kinds and locations are shared with shogiboardq-cli instead of being copied into Python."""
from __future__ import annotations

import re
from pathlib import Path

import pytest

from shogiboardq_mcp import environment
from shogiboardq_mcp.app_client import AppError

PACKAGE_DIR = Path(__file__).resolve().parents[1] / "shogiboardq_mcp"
# Failures that only the MCP server itself can have (transport, jobs, the CLI process).
PYTHON_ONLY = {"app_unavailable", "app_disconnected", "app_response_too_large", "app_error",
               "cli_not_found", "cli_failed", "job_failed", "timeout", "too_many_jobs"}


@pytest.fixture
def env(monkeypatch, cli_path):
    monkeypatch.setenv("SHOGIBOARDQ_CLI", str(cli_path))
    return environment.environment()


def test_environment_describes_the_application(env):
    assert env["max_message_bytes"] >= 1024 * 1024
    assert env["endpoint_file"].endswith("automation-endpoint.json")
    assert env["socket_path"]
    slugs = [k["slug"] for k in env["error_kinds"]]
    assert len(slugs) == len(set(slugs))
    assert {"invalid_argument", "file_exists", "not_found", "parse_error", "unsaved_changes"} <= set(slugs)


def test_python_errors_use_the_shared_words(env):
    shared = {k["slug"] for k in env["error_kinds"]}
    sources = "".join(p.read_text(encoding="utf-8") for p in PACKAGE_DIR.glob("*.py"))
    used = set(re.findall(r'ToolError\(\s*"([a-z_]+)"', sources))
    assert used, "no ToolError slugs found"
    assert used <= shared | PYTHON_ONLY, used - shared - PYTHON_ONLY
    assert not (PYTHON_ONLY & shared)


def test_app_errors_take_the_slug_from_the_application(env):
    assert AppError(-32003, "exists", {"slug": "file_exists"}).code == "file_exists"
    # Without data.slug the code is looked up in the shared table.
    assert AppError(-32006, "bad record").code == "parse_error"
    assert AppError(-32700, "not json").code == "invalid_json"
    assert AppError(-1, "odd").code == "app_error"
