"""Unit tests for the path policy (no binaries required)."""

from __future__ import annotations

import os
import sys
from pathlib import Path

import pytest

from shogiboardq_mcp import paths
from shogiboardq_mcp.errors import ToolError

# The lists of shogiboardq-cli environment (test_environment.py checks the real ones)
SUFFIXES = {
    paths.KIFU_READ: ["kifu", "kif", "ki2u", "ki2", "csa", "jkf", "json", "usen", "usi", "sfen"],
    paths.KIFU_WRITE: ["kifu", "kif", "ki2u", "ki2", "csa", "jkf", "usen", "usi", "sfen"],
    paths.KIFU_SAVE: ["kifu", "kif", "ki2u", "ki2", "csa", "jkf", "usen", "usi"],
    paths.IMAGE_WRITE: ["png", "jpg", "jpeg", "bmp", "webp"],
}


@pytest.fixture(autouse=True)
def record_suffixes(monkeypatch):
    monkeypatch.setattr(paths, "allowed_suffixes", lambda key: SUFFIXES[key])


def test_relative_paths_are_rejected(monkeypatch, tmp_path):
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", str(tmp_path))
    with pytest.raises(ToolError) as info:
        paths.resolve_read_path("relative.kif")
    assert info.value.code == "invalid_argument"


def test_read_paths_are_classified_like_the_application(monkeypatch, tmp_path):
    # The same words as InputPath::check (src/headless/inputpath.cpp): missing files are not found,
    # directories are invalid arguments.
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", str(tmp_path))
    with pytest.raises(ToolError) as info:
        paths.resolve_read_path(str(tmp_path / "missing.kif"))
    assert info.value.code == "not_found"
    (tmp_path / "folder.kif").mkdir()
    with pytest.raises(ToolError) as info:
        paths.resolve_read_path(str(tmp_path / "folder.kif"))
    assert info.value.code == "invalid_argument"


def test_paths_outside_allowed_dirs_are_rejected(monkeypatch, tmp_path):
    allowed = tmp_path / "allowed"
    allowed.mkdir()
    outside = tmp_path / "outside"
    outside.mkdir()
    (outside / "x.kif").write_text("x", encoding="utf-8")
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", str(allowed))
    with pytest.raises(ToolError) as info:
        paths.resolve_read_path(str(outside / "x.kif"))
    assert info.value.code == "not_allowed"
    with pytest.raises(ToolError):
        paths.resolve_write_path(str(outside / "new.kif"), overwrite=False)


def test_saving_the_application_record_refuses_position_only_files(monkeypatch, tmp_path):
    # kifu.save writes only the extensions of Save As; the position-only .sfen is for convert_kifu
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", str(tmp_path))
    with pytest.raises(ToolError) as info:
        paths.resolve_write_path(str(tmp_path / "game.sfen"), False, "path", paths.KIFU_SAVE)
    assert info.value.code == "invalid_argument"
    assert paths.resolve_write_path(str(tmp_path / "game.sfen"), False) == (tmp_path / "game.sfen").resolve()
    assert paths.resolve_write_path(str(tmp_path / "game.ki2u"), False, "path", paths.KIFU_SAVE)


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
    (b / "f.kif").write_text("x", encoding="utf-8")
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", os.pathsep.join([str(a), str(b)]))
    assert paths.resolve_read_path(str(b / "f.kif")) == (b / "f.kif").resolve()


def test_default_allowed_dir_is_a_dedicated_folder(monkeypatch, tmp_path):
    # Not the home directory: it holds AppData, Library and other places with settings and credentials.
    monkeypatch.delenv("SHOGIBOARDQ_ALLOWED_DIRS", raising=False)
    monkeypatch.setenv("HOME", str(tmp_path))
    monkeypatch.setenv("USERPROFILE", str(tmp_path))
    expected = (tmp_path / paths.DEFAULT_ALLOWED_DIR_NAME).resolve()
    assert paths.allowed_dirs() == [expected]
    assert expected.is_dir()
    (tmp_path / "secret.kif").write_text("x", encoding="utf-8")
    with pytest.raises(ToolError) as err:
        paths.resolve_read_path(str(tmp_path / "secret.kif"))
    assert err.value.code == "not_allowed"


@pytest.mark.parametrize("value", ["", " ", os.pathsep, "relative/dir"])
def test_a_set_but_unusable_allowed_dirs_is_an_error(monkeypatch, value):
    # It must not silently fall back to another place.
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", value)
    with pytest.raises(ToolError) as err:
        paths.allowed_dirs()
    assert err.value.code == "not_allowed"


def test_network_paths_are_refused_by_text_before_opening():
    # Path.resolve() would already connect to the server and send the user's credentials on Windows.
    roots = [Path("/allowed")]
    for raw in (r"\\attacker\share\x.kif", "//attacker/share/x.kif", r"\\?\UNC\attacker\share\x.kif"):
        assert paths._is_unc(raw, windows=True)
        assert not paths._unc_allowed(Path(raw), roots, windows=True)
    assert not paths._is_unc(r"C:\Users\me\x.kif", windows=True)
    assert paths._unc_allowed(Path(r"\\host\share\sub\x.kif"), [Path(r"\\host\share")], windows=True) or os.name != "nt"
    assert not paths._is_unc(r"\\attacker\share\x.kif", windows=False)


def test_images_are_only_replaced_when_this_server_wrote_them(monkeypatch, tmp_path, cli_path):
    # overwrite is the agent's own argument, so it must not reach the user's photos.
    monkeypatch.setenv("SHOGIBOARDQ_CLI", str(cli_path))
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", str(tmp_path))
    photo = tmp_path / "family.jpg"
    photo.write_bytes(b"x")
    with pytest.raises(ToolError) as err:
        paths.resolve_image_write_path(str(photo), overwrite=True)
    assert err.value.code == "not_allowed"
    paths.remember_image_written(photo.resolve())
    assert paths.resolve_image_write_path(str(photo), overwrite=True) == photo.resolve()
    with pytest.raises(ToolError) as err:
        paths.resolve_image_write_path(str(photo), overwrite=False)
    assert err.value.code == "file_exists"

    other = tmp_path / "other.png"
    other.write_bytes(b"x")
    monkeypatch.setenv("SHOGIBOARDQ_ALLOW_IMAGE_OVERWRITE", "1")
    assert paths.resolve_image_write_path(str(other), overwrite=True) == other.resolve()


def test_socket_and_endpoint_locations(monkeypatch, tmp_path, cli_path):
    # The locations come from shogiboardq-cli environment, which computes them like the application.
    monkeypatch.setenv("SHOGIBOARDQ_CLI", str(cli_path))
    monkeypatch.setenv("SHOGIBOARDQ_AUTOMATION_SOCKET", "/tmp/custom.sock")
    assert paths.default_socket_path() == "/tmp/custom.sock"
    monkeypatch.delenv("SHOGIBOARDQ_AUTOMATION_SOCKET")
    monkeypatch.setenv("XDG_RUNTIME_DIR", str(tmp_path))
    if not os.name == "nt":
        assert paths.default_socket_path() == str(tmp_path / "shogiboardq-p2p" / "automation.sock")
    monkeypatch.delenv("SHOGIBOARDQ_CONFIG_HOME", raising=False)
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "xdg"))
    if sys.platform.startswith("linux"):
        assert paths.endpoint_file() == tmp_path / "xdg" / "ShogiBoardQ-P2P" / "automation-endpoint.json"
    # SHOGIBOARDQ_CONFIG_HOME relocates the settings on every platform, as in the application.
    monkeypatch.setenv("SHOGIBOARDQ_CONFIG_HOME", str(tmp_path))
    assert paths.endpoint_file() == tmp_path / "ShogiBoardQ-P2P" / "automation-endpoint.json"
    assert paths.read_endpoint() is None
    (tmp_path / "ShogiBoardQ-P2P").mkdir(exist_ok=True)
    paths.endpoint_file().write_text('{"socket": "/x/y.sock", "pid": 1}', encoding="utf-8")
    assert paths.read_endpoint() == {"socket": "/x/y.sock", "pid": 1}


@pytest.mark.parametrize("existing", [False, True])
@pytest.mark.parametrize("overwrite", [False, True])
def test_output_symlink_cannot_escape_allowed_directory(monkeypatch, tmp_path, existing, overwrite):
    allowed = tmp_path / "allowed"
    allowed.mkdir()
    target = tmp_path / "outside.kif"
    if existing:
        target.write_bytes(b"unchanged")
    link = allowed / "output.kif"
    try:
        link.symlink_to(target)
    except OSError:
        pytest.skip("Symlinks are unavailable")
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", str(allowed))
    with pytest.raises(ToolError) as error:
        paths.resolve_write_path(str(link), overwrite)
    assert error.value.code == "not_allowed"
    assert target.read_bytes() == b"unchanged" if existing else not target.exists()


def test_output_symlink_inside_allowed_directory_returns_resolved_path(monkeypatch, tmp_path):
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", str(tmp_path))
    target = tmp_path / "actual.kif"
    link = tmp_path / "output.kif"
    try:
        link.symlink_to(target)
    except OSError:
        pytest.skip("Symlinks are unavailable")
    assert paths.resolve_write_path(str(link), False) == target
    target.write_bytes(b"unchanged")
    with pytest.raises(ToolError) as error:
        paths.resolve_write_path(str(link), False)
    assert error.value.code == "file_exists"
    assert paths.resolve_write_path(str(link), True) == target


@pytest.mark.parametrize("name", [".bashrc", ".netrc", ".config/gh/hosts.kif", ".local/lib/x.kif", "a/.git/x.kif"])
def test_hidden_names_below_an_allowed_directory_are_rejected(monkeypatch, tmp_path, name):
    # Hidden files and directories hold settings, credentials and start-up scripts, never records.
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", str(tmp_path))
    target = tmp_path / name
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text("token: secret", encoding="utf-8")
    for resolve in (lambda: paths.resolve_read_path(str(target)),
                    lambda: paths.resolve_write_path(str(target), overwrite=True)):
        with pytest.raises(ToolError) as error:
            resolve()
        assert error.value.code == "not_allowed"
    assert target.read_text(encoding="utf-8") == "token: secret"


def test_hidden_directory_given_as_allowed_directory_is_usable(monkeypatch, tmp_path):
    # Only the names below the allowed directory count, so a hidden directory can be allowed explicitly.
    root = tmp_path / ".kifu"
    root.mkdir()
    (root / "a.kif").write_text("x", encoding="utf-8")
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", os.pathsep.join([str(tmp_path), str(root)]))
    assert paths.resolve_read_path(str(root / "a.kif")) == (root / "a.kif").resolve()
    assert paths.resolve_output_dir(str(root)) == root.resolve()
    with pytest.raises(ToolError) as error:
        paths.resolve_output_dir(str(tmp_path / ".kifu" / ".." / ".hidden"))
    assert error.value.code == "not_allowed"


def test_symlink_to_a_hidden_file_is_rejected(monkeypatch, tmp_path):
    secret = tmp_path / ".git-credentials"
    secret.write_text("https://user:token@example.com", encoding="utf-8")
    link = tmp_path / "game.kif"
    try:
        link.symlink_to(secret)
    except OSError:
        pytest.skip("Symlinks are unavailable")
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", str(tmp_path))
    with pytest.raises(ToolError) as error:
        paths.resolve_read_path(str(link))
    assert error.value.code == "not_allowed"


@pytest.mark.parametrize("name", ["notes.txt", "hosts.yml", "start.sh", "x.pth", "config.json", "noext"])
def test_files_without_a_record_extension_are_rejected(monkeypatch, tmp_path, name):
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", str(tmp_path))
    target = tmp_path / name
    with pytest.raises(ToolError) as error:
        paths.resolve_write_path(str(target), overwrite=False)
    assert error.value.code == "invalid_argument"
    assert not target.exists()
    if name != "config.json":   # JKF is also distributed as .json, so .json is read but never written
        target.write_text("password: secret", encoding="utf-8")
        with pytest.raises(ToolError) as error:
            paths.resolve_read_path(str(target))
        assert error.value.code == "invalid_argument"


def test_record_and_image_extensions_are_accepted(monkeypatch, tmp_path):
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", str(tmp_path))
    (tmp_path / "game.JSON").write_text("{}", encoding="utf-8")
    assert paths.resolve_read_path(str(tmp_path / "game.JSON")) == (tmp_path / "game.JSON").resolve()
    assert paths.resolve_write_path(str(tmp_path / "out.KIF"), overwrite=False) == (tmp_path / "out.KIF").resolve()
    assert paths.resolve_write_path(str(tmp_path / "b.png"), False, suffixes=paths.IMAGE_WRITE) == (tmp_path / "b.png").resolve()
    with pytest.raises(ToolError):
        paths.resolve_write_path(str(tmp_path / "b.kif"), False, suffixes=paths.IMAGE_WRITE)


async def _convert_json(args):
    from shogiboardq_mcp.handlers_cli import CliTools
    tools = CliTools(None)
    with tools.kifu_input(args):
        return args


@pytest.mark.anyio
async def test_a_json_file_is_only_read_as_jkf(monkeypatch, tmp_path, cli_path):
    # .json is also used by settings and credentials; an error or a misread line must not show its contents.
    monkeypatch.setenv("SHOGIBOARDQ_CLI", str(cli_path))
    monkeypatch.setenv("SHOGIBOARDQ_ALLOWED_DIRS", str(tmp_path))
    config = tmp_path / "config.json"
    config.write_text('{"api_key":"secret"}', encoding="utf-8")
    for input_format in ("usi", "kif"):
        with pytest.raises(ToolError) as err:
            await _convert_json({"input_path": str(config), "input_format": input_format})
        assert err.value.code == "invalid_argument"
    args = await _convert_json({"input_path": str(config), "input_format": "auto"})
    assert args["input_format"] == "jkf"
