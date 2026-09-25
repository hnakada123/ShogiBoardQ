"""Shared fixtures: an isolated ShogiBoardQ config with test engines, and an MCP client session.

Environment variables (set by CTest, or manually when running pytest directly):

* ``SHOGIBOARDQ_CLI``              built ``shogiboardq-cli`` (tests needing it are skipped otherwise)
* ``SHOGIBOARDQ_EXECUTABLE``       built ``ShogiBoardQ`` (phase 2 tests)
* ``SHOGIBOARDQ_TEST_USI_ENGINE``  a USI engine for analysis (Hayanagi)
* ``SHOGIBOARDQ_TEST_MATE_ENGINE`` a ``go mate`` engine (mock_mate_engine)
"""

from __future__ import annotations

import os
import sys
from contextlib import asynccontextmanager
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[2]
FIXTURES = REPO_ROOT / "tests" / "fixtures"
PACKAGE_DIR = Path(__file__).resolve().parents[1]

sys.path.insert(0, str(PACKAGE_DIR))


@pytest.fixture(scope="session")
def anyio_backend():
    return "asyncio"


def _executable(env_name: str) -> Path | None:
    raw = os.environ.get(env_name)
    if not raw:
        return None
    path = Path(raw)
    return path if path.exists() else None


@pytest.fixture(scope="session")
def cli_path() -> Path:
    cli = _executable("SHOGIBOARDQ_CLI")
    if cli is None:
        pytest.skip("SHOGIBOARDQ_CLI is not set or does not exist (build ShogiBoardQ first)")
    return cli


@pytest.fixture(scope="session")
def app_path() -> Path:
    app = _executable("SHOGIBOARDQ_EXECUTABLE")
    if app is None:
        pytest.skip("SHOGIBOARDQ_EXECUTABLE is not set or does not exist")
    return app


@pytest.fixture(scope="session")
def test_config(tmp_path_factory) -> Path:
    """A scratch XDG_CONFIG_HOME whose ShogiBoardQ.ini registers the test engines."""
    root = tmp_path_factory.mktemp("config")
    cfg = root / "ShogiBoardQ"
    cfg.mkdir()
    engines = []
    usi = _executable("SHOGIBOARDQ_TEST_USI_ENGINE")
    mate = _executable("SHOGIBOARDQ_TEST_MATE_ENGINE")
    if usi:
        engines.append(("TestUsi", usi))
    if mate:
        engines.append(("TestMate", mate))
    lines = ["[Engines]", f"size={len(engines)}"]
    for i, (name, path) in enumerate(engines, start=1):
        lines += [f"{i}\\name={name}", f"{i}\\path={path}", f"{i}\\author=test"]
    lines += ["", "[General]", "mainWindowSize=@Size(1280 800)", ""]
    (cfg / "ShogiBoardQ.ini").write_text("\n".join(lines), encoding="utf-8")
    return root


@pytest.fixture(scope="session")
def server_env(cli_path: Path, test_config: Path, tmp_path_factory) -> dict[str, str]:
    env = dict(os.environ)
    env["SHOGIBOARDQ_CLI"] = str(cli_path)
    env["XDG_CONFIG_HOME"] = str(test_config)
    env["XDG_RUNTIME_DIR"] = str(tmp_path_factory.mktemp("runtime"))
    env["QT_QPA_PLATFORM"] = "offscreen"
    env["SHOGIBOARDQ_ALLOWED_DIRS"] = os.pathsep.join([str(tmp_path_factory.getbasetemp()), str(REPO_ROOT / "tests")])
    env["SHOGIBOARDQ_OUTPUT_DIR"] = str(tmp_path_factory.mktemp("output"))
    env["PYTHONDONTWRITEBYTECODE"] = "1"
    env.pop("SHOGIBOARDQ_AUTOMATION_SOCKET", None)
    app = _executable("SHOGIBOARDQ_EXECUTABLE")
    if app:
        env["SHOGIBOARDQ_EXECUTABLE"] = str(app)
    else:
        env.pop("SHOGIBOARDQ_EXECUTABLE", None)
    return env


@asynccontextmanager
async def mcp_session(env: dict[str, str]):
    from mcp import ClientSession, StdioServerParameters
    from mcp.client.stdio import stdio_client

    params = StdioServerParameters(command=sys.executable, args=["-m", "shogiboardq_mcp"], env=env, cwd=str(PACKAGE_DIR))
    async with stdio_client(params) as (read, write):
        async with ClientSession(read, write) as session:
            await session.initialize()
            yield session


def result_data(result):
    """Return (text, structured, is_error) from a CallToolResult."""
    text = "\n".join(c.text for c in result.content if getattr(c, "type", "") == "text")
    return text, result.structuredContent, bool(result.isError)
