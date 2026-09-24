#!/usr/bin/env python3
"""Prepare Qt notices from the corresponding sources, then stage them for release.

No source version is guessed from an installed library. The distributor supplies
the actual source archive and its provenance. Official CI uses a pinned Qt archive.
"""

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import tarfile
import tempfile
import urllib.request
from urllib.parse import quote

ROOT = Path(__file__).resolve().parents[1]


def digest(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def qt_version(value):
    if not re.fullmatch(r"6\.\d+\.\d+", value):
        raise argparse.ArgumentTypeError("Use an exact Qt 6 version, e.g. 6.7.3")
    return value


def fetch(args):
    """Fetch and verify a complete official source archive, never a moving branch."""
    name = f"qt-everywhere-src-{args.version}.tar.xz"
    url = f"https://download.qt.io/archive/qt/{args.version.rsplit('.', 1)[0]}/{args.version}/single/{name}"
    args.output.mkdir(parents=True, exist_ok=True)
    archive = args.output / name
    if archive.exists() and digest(archive) == args.sha256:
        print(f"Verified cached {archive}")
        return
    temporary = archive.with_suffix(".part")
    with urllib.request.urlopen(url, timeout=120) as response, temporary.open("wb") as target:
        shutil.copyfileobj(response, target)
    if digest(temporary) != args.sha256:
        temporary.unlink()
        raise ValueError("Qt source SHA-256 mismatch; no release may be produced")
    temporary.replace(archive)
    print(f"Verified {archive}")


def relative_member(name):
    path = PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts or "\\" in name or ":" in name:
        raise ValueError(f"Unsafe archive member: {name}")
    return path


def is_notice(path):
    name = path.name.lower()
    return (name.startswith(("license", "licence", "copying", "copyright", "notice", "authors"))
            or "LICENSES" in path.parts or name == "qt_attribution.json")


def prepare(args):
    """Keep upstream texts byte-for-byte, including licenses referenced by metadata."""
    if args.output.exists():
        raise ValueError(f"Output already exists: {args.output}; use a new directory")
    checksum = digest(args.archive)
    if args.sha256 and checksum != args.sha256:
        raise ValueError("Qt source SHA-256 mismatch")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=args.output.parent) as temporary:
        output = Path(temporary) / "licenses"
        shutil.copytree(ROOT / "resources/licenses", output)
        required_files = set()
        copied_files = set()
        version_found = False
        with tarfile.open(args.archive, "r|*") as archive:
            for member in archive:
                if not member.isfile():
                    continue
                path = PurePosixPath(member.name)
                is_version = path.parts[-2:] == ("qtbase", ".cmake.conf")
                if not is_version and not is_notice(path):
                    continue
                path = relative_member(member.name)
                # Both complete Qt and custom downstream archives must identify Qt Base.
                if is_version:
                    config = archive.extractfile(member).read().decode("utf-8")
                    match = re.search(r'set\(QT_REPO_MODULE_VERSION\s+"([^"]+)"\)', config)
                    if not match or match[1] != args.version:
                        raise ValueError("Qt Base source version differs from --version")
                    version_found = True
                if not is_notice(path):
                    continue
                data = archive.extractfile(member).read()
                destination = output / "qt" / path
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(data)
                copied_files.add(path)
                if path.name == "qt_attribution.json":
                    # Some upstream attribution strings contain literal line breaks.
                    # Preserve their original bytes; accept those strings when finding references.
                    try:
                        entries = json.loads(data, strict=False)
                    except ValueError as error:
                        raise ValueError(f"Invalid attribution metadata in {path}: {error}") from error
                    if isinstance(entries, dict):
                        entries = [entries]
                    for entry in entries:
                        license_files = entry.get("LicenseFiles", entry.get("LicenseFile", []))
                        if isinstance(license_files, str):
                            license_files = [license_files]
                        for license_file in license_files:
                            # Upstream metadata uses ../ for licenses shared by components.
                            parts = list(path.parent.parts)
                            ref = PurePosixPath(license_file)
                            if ref.is_absolute() or "\\" in license_file or ":" in license_file:
                                raise ValueError(f"Unsafe LicenseFile: {license_file}")
                            for part in ref.parts:
                                if part == "..":
                                    if len(parts) <= 1:
                                        raise ValueError(f"LicenseFile escapes archive: {license_file}")
                                    parts.pop()
                                else:
                                    parts.append(part)
                            required_files.add(PurePosixPath(*parts))
        if not version_found:
            raise ValueError("Source archive is missing qtbase/.cmake.conf")
        missing = required_files - copied_files
        if missing:
            with tarfile.open(args.archive, "r|*") as archive:
                for member in archive:
                    path = PurePosixPath(member.name)
                    if path in missing and member.isfile():
                        destination = output / "qt" / path
                        destination.parent.mkdir(parents=True, exist_ok=True)
                        destination.write_bytes(archive.extractfile(member).read())
                        missing.remove(path)
        if missing:
            raise ValueError("Missing referenced licenses: " + ", ".join(map(str, sorted(missing))))
        if not any(path.name == "LGPL-3.0-only.txt" for path in copied_files):
            raise ValueError("Qt source archive does not contain LGPL-3.0-only.txt")
        if not any(path.name == "GPL-3.0-only.txt" for path in copied_files):
            raise ValueError("Qt source archive does not contain GPL-3.0-only.txt")
        write_json(output / "QT-SOURCE.json", {
            "qt_version": args.version, "archive": args.archive.name,
            "sha256": checksum, "source_url": args.source_url,
            "provenance": args.provenance,
            "release_sources": "https://github.com/hnakada123/ShogiBoardQ/releases",
        })
        files = sorted((output / "qt").rglob("*"))
        index = "# Qt license and attribution files\n\n"
        index += "Texts are copied unchanged from the identified Qt source archive.\n"
        index += "This inventory includes components not necessarily enabled in this build.\n\n"
        index += "\n".join(f"- [{p.relative_to(output).as_posix()}]({quote(p.relative_to(output).as_posix())})"
                           for p in files if p.is_file()) + "\n"
        (output / "QT-NOTICES.md").write_text(index, encoding="utf-8")
        write_json(output / "FILES.json", {
            p.relative_to(output).as_posix(): digest(p)
            for p in sorted(output.rglob("*")) if p.is_file()
        })
        shutil.move(str(output), args.output)
    print(f"Prepared {args.output}")


def stage(args):
    source = args.notices
    if not (source / "QT-SOURCE.json").is_file():
        raise ValueError("Qt notices are missing. See docs/dev/qt-licensing.md; "
                         "set SHOGIBOARDQ_QT_LICENSE_DIR to prepared matching notices.")
    manifest = json.loads((source / "QT-SOURCE.json").read_text(encoding="utf-8"))
    build = json.loads((args.build_dir / "qt-build.json").read_text(encoding="utf-8"))
    if build["qt_version"] != manifest["qt_version"]:
        raise ValueError(f"Qt version mismatch: build={build['qt_version']}, source={manifest['qt_version']}")
    if build["qt_library_type"] != "SHARED_LIBRARY":
        raise ValueError("These release instructions require shared Qt libraries")
    inventory = json.loads((source / "FILES.json").read_text(encoding="utf-8"))
    for required in ("NOTICE.md", "SOURCE_CODE.md", "GPL-3.0.txt", "LGPL-3.0.txt",
                     "QT-SOURCE.json", "QT-NOTICES.md"):
        if required not in inventory:
            raise ValueError(f"Missing required notice: {required}")
    for name, checksum in inventory.items():
        relative_member(name)
        if digest(source / name) != checksum:
            raise ValueError(f"Notice changed since preparation: {name}")
    shutil.copytree(source, args.destination, dirs_exist_ok=True)
    shutil.copyfile(args.build_dir / "qt-build.json", args.destination / "BUILD.json")
    # Record actual Qt SDK configuration alongside the source identification.
    cache = (args.build_dir / "CMakeCache.txt").read_text(encoding="utf-8")
    match = re.search(r"^Qt6_DIR:[^=]+=(.+)$", cache, re.MULTILINE)
    if match:
        qt_cmake = Path(match[1])
        for name in ("Qt6ConfigVersion.cmake", "Qt6Config.cmake"):
            candidate = qt_cmake / name
            if candidate.is_file():
                destination = args.destination / "qt-sdk" / name
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(candidate, destination)
        # Installed features and build configuration help rebuild a compatible Qt.
        for module in qt_cmake.parent.glob("Qt6*"):
            for candidate in module.glob("*ConfigExtras.cmake"):
                destination = args.destination / "qt-sdk" / candidate.name
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(candidate, destination)
    print(f"Staged notices for Qt {build['qt_version']} in {args.destination}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    download = commands.add_parser("fetch")
    download.add_argument("--version", type=qt_version, required=True)
    download.add_argument("--sha256", required=True)
    download.add_argument("--output", type=Path, required=True)
    prepare_parser = commands.add_parser("prepare")
    prepare_parser.add_argument("--version", type=qt_version, required=True)
    prepare_parser.add_argument("--archive", type=Path, required=True)
    prepare_parser.add_argument("--sha256")
    prepare_parser.add_argument("--source-url", required=True)
    prepare_parser.add_argument("--provenance", required=True,
                                help="Identify supplier, patches and build instructions; do not guess")
    prepare_parser.add_argument("--output", type=Path, required=True)
    staging = commands.add_parser("stage")
    staging.add_argument("--build-dir", type=Path, default=Path("build"))
    staging.add_argument("--notices", type=Path, required=True)
    staging.add_argument("--destination", type=Path, required=True)
    args = parser.parse_args()
    try:
        {"fetch": fetch, "prepare": prepare, "stage": stage}[args.command](args)
    except (OSError, ValueError, tarfile.TarError) as error:
        parser.exit(1, f"Qt license preparation failed: {error}\n")


if __name__ == "__main__":
    main()
