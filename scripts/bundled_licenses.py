#!/usr/bin/env python3
"""Stage license notices of the non-Qt libraries bundled in the Linux AppImage.

linuxdeploy copies shared libraries from the build system. This script finds the system
package of every bundled library and plugin, copies the package's license texts into
``third-party/`` and writes ``THIRD-PARTY-NOTICES.md`` with versions and source locations.
Qt itself is covered by qt_licenses.py. Only Arch Linux (pacman) packages are supported;
on other systems packaging stops instead of shipping libraries without their notices.
"""

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess

# Arch のライセンス欄が SPDX 形式でないパッケージは、上流の表記で補う（libasyncns・libidn2 は上流の
# ヘッダー、ほかは同じパッケージの 2026 年の Arch の表記）。古いリポジトリ（Arch Linux Archive）では
# 表記が古いものが多い。キーは (パッケージ名, 元の表記) とし、表記が変わったら補正しない。
LICENSE_OVERRIDES = {
    ("libasyncns", "LGPL"): "LGPL-2.1-or-later",
    ("libidn2", "GPL2  LGPL3"): "LGPL-3.0-or-later OR GPL-2.0-or-later",
    ("fcitx5-qt", "GPL"): "LGPL-2.1-or-later  BSD-3-Clause  GPL-2.0-or-later",
    ("keyutils", "GPL2  LGPL2.1"): "GPL-2.0-or-later  LGPL-2.1-or-later",
    ("lame", "LGPL"): "LGPL-2.0-only",
    ("libpulse", "LGPL"): "LGPL-2.1-or-later",
    ("libunistring", "GPL"): "GPL-2.0-or-later  LGPL-3.0-or-later",
    ("mpg123", "LGPL2.1"): "LGPL-2.1-only",
    ("flac", "BSD  GPL"): "BSD-3-Clause  GPL-2.0-or-later",
    ("gcc-libs", "GPL-3.0-with-GCC-exception  GFDL-1.3-or-later"):
        "GPL-3.0-or-later WITH GCC-exception-3.1  GFDL-1.3-or-later",
}
# パッケージに含まれない文書（実際の著作権表示を付けた BSD の本文など）を、ここから加える。
EXTRA_TEXTS = Path(__file__).resolve().parent / "license-texts"
# GPL 系の本文はパッケージ固有の文書に含まれないことが多いので、共通の本文が見つからなければ止める。
# 古い表記（GPL・GPL2・LGPL2.1 など）も対象にする。
COPYLEFT = re.compile(r"(A|L)?GPL")
OPERATORS = {"AND", "OR", "WITH"}


def license_expression(name, field):
    return LICENSE_OVERRIDES.get((name, field), field)


def is_qt_package(name):
    """Qt packages are covered by the Qt notices (qt_licenses.py)."""
    return name.startswith("qt6-")


def license_ids(expression):
    """SPDX identifiers and exceptions named in a license expression."""
    return [token for token in re.findall(r"[A-Za-z0-9][A-Za-z0-9.+-]*", expression)
            if token not in OPERATORS]


class Pacman:
    """Package queries on Arch Linux."""

    licenses = Path("/usr/share/licenses")

    def __init__(self, qt_plugins):
        if shutil.which("pacman") is None:
            raise ValueError("Bundled library notices need pacman (Arch Linux); "
                             "other distributions are not supported yet")
        self.qt_plugins = qt_plugins
        self.cache = {}
        for line in self.run("ldconfig", "-p").splitlines():
            match = re.match(r"\s+(\S+) \(([^)]*)\) => (\S+)", line)
            if match and "x86-64" in match[2]:
                self.cache.setdefault(match[1], Path(match[3]))

    @staticmethod
    def run(*command):
        environment = dict(os.environ, LANG="C", LC_ALL="C")
        return subprocess.run(command, check=True, capture_output=True, text=True,
                              env=environment).stdout

    def host_path(self, bundled, appdir):
        """The system file that linuxdeploy copied to ``bundled``."""
        relative = bundled.relative_to(appdir / "usr")
        if relative.parts[0] == "plugins":
            candidates = [self.qt_plugins / Path(*relative.parts[1:])]
        else:
            # 一部の内部ライブラリ（libpulsecommon など）はサブディレクトリにある。
            candidates = [self.cache.get(bundled.name), Path("/usr/lib") / bundled.name,
                          *sorted(Path("/usr/lib").glob("*/" + bundled.name))]
        for candidate in candidates:
            if candidate and candidate.is_file():
                return candidate
        raise ValueError(f"Cannot find the system file of {relative}")

    def owners(self, paths):
        owners = {}
        for line in self.run("pacman", "-Qo", *map(str, paths)).splitlines():
            match = re.match(r"(\S+) is owned by (\S+) (\S+)$", line)
            if match:
                owners[Path(match[1])] = match[2]
        return owners

    def package(self, name):
        fields = {}
        for line in self.run("pacman", "-Qi", name).splitlines():
            key, separator, value = line.partition(":")
            if separator and not line.startswith(" "):
                fields[key.strip()] = value.strip()
        version = fields["Version"]
        base = name
        desc = Path("/var/lib/pacman/local") / f"{name}-{version}" / "desc"
        if desc.is_file():
            lines = desc.read_text(encoding="utf-8").splitlines()
            if "%BASE%" in lines:
                base = lines[lines.index("%BASE%") + 1]
        return {"version": version, "license": fields["Licenses"], "url": fields.get("URL", ""),
                "source": "https://gitlab.archlinux.org/archlinux/packaging/packages/"
                          f"{base}/-/tree/{version.replace(':', '-')}"}

    def license_files(self, name):
        directory = self.licenses / name
        return sorted(p for p in directory.iterdir() if p.is_file()) if directory.is_dir() else []

    def spdx_text(self, identifier):
        for candidate in (self.licenses / "spdx" / f"{identifier}.txt",
                          self.licenses / "spdx" / "exceptions" / f"{identifier}.txt"):
            if candidate.is_file():
                return candidate
        return None


def bundled_files(appdir):
    usr = appdir / "usr"
    return sorted(p for pattern in ("lib/*.so*", "plugins/*/*.so")
                  for p in usr.glob(pattern) if p.is_file())


def stage(appdir, destination, system, extra_texts=EXTRA_TEXTS):
    files = bundled_files(appdir)
    hosts = {path: system.host_path(path, appdir) for path in files}
    owners = system.owners(sorted(set(hosts.values())))
    packages = {}
    for path, host in hosts.items():
        if host not in owners:
            raise ValueError(f"No package owns {host} (bundled as {path.relative_to(appdir)})")
        packages.setdefault(owners[host], []).append(path.name)

    third_party = destination / "third-party"
    shutil.rmtree(third_party, ignore_errors=True)
    sections = []
    for name in sorted(packages):
        if is_qt_package(name):
            continue
        info = system.package(name)
        expression = license_expression(name, info["license"])
        texts = []
        extra = extra_texts / name
        package_texts = list(system.license_files(name))
        if extra.is_dir():
            package_texts += sorted(p for p in extra.iterdir() if p.is_file())
        for text in package_texts:
            target = third_party / name / text.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(text, target)
            texts.append(target)
        for identifier in license_ids(expression):
            text = system.spdx_text(identifier)
            if not text and COPYLEFT.match(identifier):
                raise ValueError(f"No license text for {identifier} of bundled package {name} ({expression})")
            if text:
                # 共通のライセンス本文は spdx/ に1つだけ置き、各ライブラリから参照する。
                target = third_party / "spdx" / text.name
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(text, target)
                if target not in texts:
                    texts.append(target)
        if not texts:
            raise ValueError(f"No license text for bundled package {name} ({expression})")
        links = ", ".join(f"[{p.relative_to(destination).as_posix()}]({p.relative_to(destination).as_posix()})"
                          for p in texts)
        lines = [f"## {name} {info['version']}", "",
                 "- Files: " + ", ".join(f"`{file}`" for file in sorted(packages[name])),
                 f"- License: {expression}",
                 f"- License texts: {links}"]
        if info["url"]:
            lines.append(f"- Upstream: [{info['url']}]({info['url']})")
        lines.append(f"- Source: [{info['source']}]({info['source']})")
        sections.append("\n".join(lines))

    index = ("# Bundled library licenses\n\n"
             "Besides Qt, this AppImage bundles the libraries below, taken from Arch Linux packages.\n"
             "Each library is distributed under its own license; the license texts are in `third-party/`.\n"
             "Corresponding source is available from the Arch Linux package under \"Source\"\n"
             "(PKGBUILD and patches, which name the upstream source archive) and from upstream.\n"
             "Qt's notices are listed separately in \"Third-party licenses in Qt\".\n\n"
             + "\n\n".join(sections) + "\n")
    destination.mkdir(parents=True, exist_ok=True)
    (destination / "THIRD-PARTY-NOTICES.md").write_text(index, encoding="utf-8")
    return sorted(name for name in packages if not is_qt_package(name))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--appdir", type=Path, required=True)
    parser.add_argument("--qt-plugins", type=Path, required=True,
                        help="Qt plugin directory the AppDir plugins were copied from")
    parser.add_argument("--destination", type=Path, required=True)
    args = parser.parse_args()
    try:
        packages = stage(args.appdir, args.destination, Pacman(args.qt_plugins))
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Bundled library notices failed: {error}\n")
    print(f"Staged notices of {len(packages)} bundled packages in {args.destination}")


if __name__ == "__main__":
    main()
