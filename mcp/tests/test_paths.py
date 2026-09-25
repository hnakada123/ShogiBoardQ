"""Unit tests for the path policy (no binaries required)."""

from __future__ import annotations

import os
from pathlib import Path

import pytest

from shogiboardq_mcp import paths
from shogiboardq_mcp.errors import ToolError


def test_relative_paths_are_rejected(monkeypatch, tmp_path):
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", str(tmp_path))
    with pytest.raises(ToolError) as info:
        paths.resolve_read_path("relative.kif")
    assert info.value.code == "invalid_path"


def test_paths_outside_allowed_dirs_are_rejected(monkeypatch, tmp_path):
    allowed = tmp_path / "allowed"
    allowed.mkdir()
    outside = tmp_path / "outside"
    outside.mkdir()
    (outside / "x.kif").write_text("x", encoding="utf-8")
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", str(allowed))
    with pytest.raises(ToolError) as info:
        paths.resolve_read_path(str(outside / "x.kif"))
    assert info.value.code == "path_not_allowed"
    with pytest.raises(ToolError):
        paths.resolve_write_path(str(outside / "new.kif"), overwrite=False)


def test_overwrite_policy(monkeypatch, tmp_path):
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", str(tmp_path))
    target = tmp_path / "out.kif"
    assert paths.resolve_write_path(str(target), overwrite=False) == target.resolve()
    target.write_text("x", encoding="utf-8")
    with pytest.raises(ToolError) as info:
        paths.resolve_write_path(str(target), overwrite=False)
    assert info.value.code == "file_exists"
    assert paths.resolve_write_path(str(target), overwrite=True) == target.resolve()
    with pytest.raises(ToolError):
        paths.resolve_write_path(str(tmp_path / "missing" / "out.kif"), overwrite=False)


def test_multiple_allowed_dirs(monkeypatch, tmp_path):
    a = tmp_path / "a"
    b = tmp_path / "b"
    a.mkdir()
    b.mkdir()
    (b / "f.txt").write_text("x", encoding="utf-8")
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", os.pathsep.join([str(a), str(b)]))
    assert paths.resolve_read_path(str(b / "f.txt")) == (b / "f.txt").resolve()


def test_default_allowed_dir_is_home(monkeypatch):
    monkeypatch.delenv("SHOGIBOARDQ_ALLOWED_DIRS", raising=False)
    assert paths.allowed_dirs() == [Path.home().resolve()]


def test_socket_and_endpoint_locations(monkeypatch, tmp_path):
    monkeypatch.setenv("SHOGIBOARDQ_AUTOMATION_SOCKET", "/tmp/custom.sock")
    assert paths.default_socket_path() == "/tmp/custom.sock"
    monkeypatch.delenv("SHOGIBOARDQ_AUTOMATION_SOCKET")
    monkeypatch.setenv("XDG_RUNTIME_DIR", str(tmp_path))
    if not os.name == "nt":
        assert paths.default_socket_path() == str(tmp_path / "shogiboardq" / "automation.sock")
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path))
    if not os.name == "nt" and os.uname().sysname != "Darwin":
        assert paths.endpoint_file() == tmp_path / "ShogiBoardQ" / "automation-endpoint.json"
    assert paths.read_endpoint() is None
    (tmp_path / "ShogiBoardQ").mkdir()
    paths.endpoint_file().write_text('{"socket": "/x/y.sock", "pid": 1}', encoding="utf-8")
    if not os.name == "nt" and os.uname().sysname != "Darwin":
        assert paths.read_endpoint() == {"socket": "/x/y.sock", "pid": 1}
