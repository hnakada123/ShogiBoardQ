"""File path policy and endpoint discovery.

* File arguments must be absolute and inside one of the allowed directories
  (``SHOGIBOARDQ_ALLOWED_DIRS``, default: the user's home directory).
* Below an allowed directory, no name may start with ``.``: hidden files and directories hold settings,
  credentials and start-up scripts, never records.
* Files must have the extension of a record (or an image, for board images). The lists come from
  ``shogiboardq-cli environment``, so an agent can neither read nor write other kinds of files.
* Existing files are only replaced when the caller passes ``overwrite``.
* The automation socket of a running ShogiBoardQ is discovered through the
  endpoint file the application writes next to its settings.
"""

from __future__ import annotations

import json
import os
import tempfile
from pathlib import Path

from .environment import environment
from .errors import ToolError


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


# The extension lists of shogiboardq-cli environment
KIFU_READ = "kifu_read_suffixes"
KIFU_WRITE = "kifu_write_suffixes"
IMAGE_WRITE = "image_write_suffixes"


def _check_allowed(path: Path) -> Path:
    resolved = path.resolve()
    inside = False
    for root in allowed_dirs():
        try:
            parts = resolved.relative_to(root).parts
        except ValueError:
            continue
        inside = True
        if not any(part.startswith(".") for part in parts):
            return resolved
    if inside:
        raise ToolError(
            "not_allowed",
            f"{path} is a hidden file or inside a hidden directory (a name starting with '.'). "
            "Add the directory itself to SHOGIBOARDQ_ALLOWED_DIRS to use it.",
        )
    roots = os.pathsep.join(str(r) for r in allowed_dirs())
    raise ToolError(
        "not_allowed",
        f"{path} is outside the allowed directories ({roots}). "
        "Set SHOGIBOARDQ_ALLOWED_DIRS to widen the policy.",
    )


def allowed_suffixes(suffixes: str) -> list[str]:
    """The extensions (without the dot) of one list of shogiboardq-cli environment."""
    return [str(s).lower() for s in environment()[suffixes]]


def _check_suffix(path: Path, raw: str, what: str, suffixes: str) -> None:
    allowed = allowed_suffixes(suffixes)
    if path.suffix[1:].lower() not in allowed:
        raise ToolError(
            "invalid_argument",
            f"{what} must have one of the extensions {', '.join('.' + s for s in allowed)}, got: {raw}",
        )


def _require_absolute(raw: str, what: str) -> Path:
    if not raw or not raw.strip():
        raise ToolError("invalid_argument", f"{what} must not be empty")
    path = Path(raw)
    if not path.is_absolute():
        raise ToolError("invalid_argument", f"{what} must be an absolute path, got: {raw}")
    return path


def resolve_read_path(raw: str, what: str = "path", suffixes: str = KIFU_READ) -> Path:
    path = _check_allowed(_require_absolute(raw, what))
    _check_suffix(path, raw, what, suffixes)
    if not path.exists():
        raise ToolError("not_found", f"{what} does not exist: {raw}")
    if not path.is_file():
        raise ToolError("invalid_argument", f"{what} is not a regular file: {raw}")
    return path


def resolve_write_path(raw: str, overwrite: bool, what: str = "output_path", suffixes: str = KIFU_WRITE) -> Path:
    # The error kinds match OutputPath::check (src/headless/outputpath.cpp), which the CLI and kifu.save use.
    path = _require_absolute(raw, what)
    resolved = _check_allowed(path)
    _check_suffix(resolved, raw, what, suffixes)
    if resolved.exists():
        if not resolved.is_file():
            raise ToolError("invalid_argument", f"{what} is not a regular file: {raw}")
        if not overwrite:
            raise ToolError(
                "file_exists",
                f"{what} already exists: {raw}. Pass overwrite=true to replace it.",
            )
    if not resolved.parent.is_dir():
        raise ToolError("not_found", f"The directory of {what} does not exist: {resolved.parent}")
    return resolved


def resolve_output_dir(raw: str | None) -> Path:
    if raw:
        path = _check_allowed(_require_absolute(raw, "output_dir"))
        if not path.is_dir():
            raise ToolError("invalid_argument", f"output_dir is not a directory: {raw}")
        return path
    return default_output_dir()


_private_output_dir: Path | None = None


def default_output_dir() -> Path:
    """SHOGIBOARDQ_OUTPUT_DIR, or a folder only this user can open, made once per server.

    A fixed name in the shared temp directory could be created first by another user, who could then read
    the screenshots or redirect them through links in it.
    """
    global _private_output_dir
    raw = os.environ.get("SHOGIBOARDQ_OUTPUT_DIR")
    if raw:
        path = Path(raw).expanduser()
        path.mkdir(parents=True, exist_ok=True)
        return path
    if _private_output_dir is None or not _private_output_dir.is_dir():
        _private_output_dir = Path(tempfile.mkdtemp(prefix="shogiboardq-output-"))
    return _private_output_dir


def endpoint_file() -> Path:
    """The endpoint file a running ShogiBoardQ writes next to its settings (from shogiboardq-cli environment)."""
    return Path(environment()["endpoint_file"])


def read_endpoint() -> dict | None:
    try:
        with endpoint_file().open("r", encoding="utf-8") as fh:
            data = json.load(fh)
    except (OSError, ValueError, ToolError):
        return None
    return data if isinstance(data, dict) else None


def default_socket_path() -> str:
    env = os.environ.get("SHOGIBOARDQ_AUTOMATION_SOCKET")
    if env:
        return env
    return str(environment()["socket_path"])
