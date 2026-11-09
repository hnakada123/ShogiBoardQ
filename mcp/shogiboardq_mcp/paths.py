"""File path policy and endpoint discovery.

* File arguments must be absolute and inside one of the allowed directories
  (``SHOGIBOARDQ_ALLOWED_DIRS``, default: the folder ``~/ShogiBoardQ-MCP``, created when first needed. The home
  directory would include places such as ``AppData`` and ``Library`` that hold settings and credentials).
* Below an allowed directory, no name may start with ``.``: hidden files and directories hold settings,
  credentials and start-up scripts, never records.
* Files must have the extension of a record (or an image, for board images). The lists come from
  ``shogiboardq-cli environment``, so an agent can neither read nor write other kinds of files.
  Converting writes ``kifu_write_suffixes`` (with the position-only ``.sfen``); saving the application's
  record writes ``kifu_save_suffixes``, the extensions of the application's Save As.
* Existing files are only replaced when the caller passes ``overwrite``. ``overwrite`` is an argument of the agent,
  so images are only replaced when this server wrote them (or the operator sets
  ``SHOGIBOARDQ_ALLOW_IMAGE_OVERWRITE=1``).
* The automation socket of a running ShogiBoardQ is discovered through the
  endpoint file the application writes next to its settings.
"""

from __future__ import annotations

import json
import ntpath
import os
import tempfile
from pathlib import Path

from .environment import environment
from .errors import ToolError


DEFAULT_ALLOWED_DIR_NAME = "ShogiBoardQ-MCP"


def default_allowed_dir() -> Path:
    """The folder files are read from and written to when the operator chose none (made once, private to the user)."""
    path = Path.home() / DEFAULT_ALLOWED_DIR_NAME
    try:
        path.mkdir(mode=0o700, exist_ok=True)
    except OSError as exc:
        raise ToolError("not_allowed", f"Could not create the default folder {path}: {exc}. "
                        "Set SHOGIBOARDQ_ALLOWED_DIRS.") from exc
    return path


def allowed_dirs() -> list[Path]:
    raw = os.environ.get("SHOGIBOARDQ_ALLOWED_DIRS")
    if raw is None:
        dirs = [default_allowed_dir()]
    else:
        dirs = [Path(p).expanduser() for p in raw.split(os.pathsep) if p.strip()]
        # A set value that names nothing, or a relative path, must not silently fall back to another place.
        if not dirs or any(not d.is_absolute() for d in dirs):
            raise ToolError("not_allowed", f"SHOGIBOARDQ_ALLOWED_DIRS must list absolute directories, got: {raw!r}")
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
KIFU_SAVE = "kifu_save_suffixes"   # what the application's kifu.save writes (no position-only .sfen)
IMAGE_WRITE = "image_write_suffixes"


def _is_unc(raw: str, windows: bool | None = None) -> bool:
    """Whether a path is a Windows network path (``\\\\server\\share``, ``\\\\?\\UNC\\...``)."""
    if not (os.name == "nt" if windows is None else windows):
        return False
    return ntpath.splitdrive(raw)[0].startswith(("\\\\", "//"))


def _unc_allowed(path: Path, roots: list[Path], windows: bool | None = None) -> bool:
    """Compare a network path with the allowed directories by its text only: opening it would already connect."""
    if not _is_unc(str(path), windows):
        return True
    text = ntpath.normcase(ntpath.normpath(str(path)))
    for root in roots:
        prefix = ntpath.normcase(ntpath.normpath(str(root))).rstrip("\\") + "\\"
        if text.startswith(prefix):
            return True
    return False


def _check_allowed(path: Path) -> Path:
    # Path.resolve() opens the path. On Windows a network path would connect to that server and send the user's
    # credentials before the checks below run, so it is compared first.
    if not _unc_allowed(path, allowed_dirs()):
        raise ToolError("not_allowed", f"{path} is a network path outside the allowed directories.")
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
    # The error kinds match InputPath::check (src/headless/inputpath.cpp), which the CLI and kifu.load use.
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


_images_written: set[Path] = set()


def resolve_image_write_path(raw: str, overwrite: bool, what: str = "output_path") -> Path:
    """Like resolve_write_path for an image. ``overwrite`` is the agent's own argument, so it only replaces what this
    server wrote; the user's photos stay safe unless the operator allows it."""
    resolved = resolve_write_path(raw, overwrite, what, suffixes=IMAGE_WRITE)
    if resolved.exists() and resolved not in _images_written \
            and os.environ.get("SHOGIBOARDQ_ALLOW_IMAGE_OVERWRITE") != "1":
        raise ToolError("not_allowed", f"{what} already exists and was not written by this server: {raw}. "
                        "Choose a new file name, or the operator may set SHOGIBOARDQ_ALLOW_IMAGE_OVERWRITE=1.")
    return resolved


def remember_image_written(path: Path) -> None:
    _images_written.add(path)


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
