"""File path policy and endpoint discovery.

* File arguments must be absolute and inside one of the allowed directories
  (``SHOGIBOARDQ_ALLOWED_DIRS``, default: the user's home directory).
* Existing files are only replaced when the caller passes ``overwrite``.
* The automation socket of a running ShogiBoardQ is discovered through the
  endpoint file the application writes next to its settings.
"""

from __future__ import annotations

import getpass
import json
import os
import sys
import tempfile
from pathlib import Path

from .errors import ToolError

ENDPOINT_FILE_NAME = "automation-endpoint.json"


def allowed_dirs() -> list[Path]:
    raw = os.environ.get("SHOGIBOARDQ_ALLOWED_DIRS", "")
    dirs = [Path(p).expanduser() for p in raw.split(os.pathsep) if p.strip()]
    if not dirs:
        dirs = [Path.home()]
    resolved: list[Path] = []
    for d in dirs:
        try:
            resolved.append(d.resolve())
        except OSError:
            resolved.append(d)
    return resolved


def _check_allowed(path: Path) -> Path:
    resolved = path.resolve()
    for root in allowed_dirs():
        try:
            resolved.relative_to(root)
            return resolved
        except ValueError:
            continue
    roots = os.pathsep.join(str(r) for r in allowed_dirs())
    raise ToolError(
        "path_not_allowed",
        f"{path} is outside the allowed directories ({roots}). "
        "Set SHOGIBOARDQ_ALLOWED_DIRS to widen the policy.",
    )


def _require_absolute(raw: str, what: str) -> Path:
    if not raw or not raw.strip():
        raise ToolError("invalid_path", f"{what} must not be empty")
    path = Path(raw)
    if not path.is_absolute():
        raise ToolError("invalid_path", f"{what} must be an absolute path, got: {raw}")
    return path


def resolve_read_path(raw: str, what: str = "path") -> Path:
    path = _check_allowed(_require_absolute(raw, what))
    if not path.exists():
        raise ToolError("file_not_found", f"{what} does not exist: {raw}")
    if not path.is_file():
        raise ToolError("invalid_path", f"{what} is not a regular file: {raw}")
    return path


def resolve_write_path(raw: str, overwrite: bool, what: str = "output_path") -> Path:
    path = _require_absolute(raw, what)
    resolved = _check_allowed(path.parent) / path.name
    if resolved.exists():
        if resolved.is_dir():
            raise ToolError("invalid_path", f"{what} is a directory: {raw}")
        if not overwrite:
            raise ToolError(
                "file_exists",
                f"{what} already exists: {raw}. Pass overwrite=true to replace it.",
            )
    if not resolved.parent.is_dir():
        raise ToolError("invalid_path", f"The directory of {what} does not exist: {resolved.parent}")
    return resolved


def resolve_output_dir(raw: str | None) -> Path:
    if raw:
        path = _check_allowed(_require_absolute(raw, "output_dir"))
        if not path.is_dir():
            raise ToolError("invalid_path", f"output_dir is not a directory: {raw}")
        return path
    return default_output_dir()


def default_output_dir() -> Path:
    raw = os.environ.get("SHOGIBOARDQ_OUTPUT_DIR")
    path = Path(raw).expanduser() if raw else Path(tempfile.gettempdir()) / "shogiboardq-mcp"
    path.mkdir(parents=True, exist_ok=True)
    return path


def app_config_dir() -> Path:
    """Mirror of Qt's ``QStandardPaths::AppConfigLocation`` for ShogiBoardQ."""
    if sys.platform.startswith("win"):
        base = os.environ.get("LOCALAPPDATA") or str(Path.home() / "AppData" / "Local")
        return Path(base) / "ShogiBoardQ"
    if sys.platform == "darwin":
        return Path.home() / "Library" / "Preferences" / "ShogiBoardQ"
    base = os.environ.get("XDG_CONFIG_HOME") or str(Path.home() / ".config")
    return Path(base) / "ShogiBoardQ"


def endpoint_file() -> Path:
    return app_config_dir() / ENDPOINT_FILE_NAME


def read_endpoint() -> dict | None:
    try:
        with endpoint_file().open("r", encoding="utf-8") as fh:
            data = json.load(fh)
    except (OSError, ValueError):
        return None
    return data if isinstance(data, dict) else None


def default_socket_path() -> str:
    env = os.environ.get("SHOGIBOARDQ_AUTOMATION_SOCKET")
    if env:
        return env
    user = _user_name()
    if sys.platform.startswith("win"):
        return rf"\\.\pipe\shogiboardq-automation-{user}"
    runtime = os.environ.get("XDG_RUNTIME_DIR") or os.path.join(tempfile.gettempdir(), f"runtime-{user}")
    return os.path.join(runtime, "shogiboardq", "automation.sock")


def _user_name() -> str:
    try:
        return getpass.getuser()
    except Exception:  # pragma: no cover - platform specific
        return "user"
