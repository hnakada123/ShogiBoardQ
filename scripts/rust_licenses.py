#!/usr/bin/env python3
"""Write the license notices of the Rust crates built into the P2P communication process.

``shogiboardq-peer`` links its Rust dependencies statically, so every distribution of it has to
carry their copyright notices and license texts. This script lists the crates the peer depends on
(for all release targets, from ``cargo tree`` with the versions pinned in ``Cargo.lock``), collects
the license texts each crate ships, and writes one self-contained Markdown document. The document is
committed as ``resources/licenses/RUST-NOTICES.md`` and built into the application, where the About
dialog shows it; ``--check`` verifies that the committed document still matches ``Cargo.lock``.

    python3 scripts/rust_licenses.py            # rewrite resources/licenses/RUST-NOTICES.md
    python3 scripts/rust_licenses.py --check    # exit 1 when it is out of date

The crate sources must be in the local cargo registry (run ``cargo fetch --locked`` first).
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_OUTPUT = ROOT / "resources/licenses/RUST-NOTICES.md"
PACKAGE = "shogiboardq-peer"
# 配布する（または利用者がビルドする）OS とアーキテクチャ。OS ごとに依存が違うので、全部の和集合を載せる
# （手元の OS によらず同じ文書になる）。
TARGETS = (
    "x86_64-unknown-linux-gnu",
    "aarch64-unknown-linux-gnu",
    "x86_64-pc-windows-msvc",
    "aarch64-pc-windows-msvc",
    "aarch64-apple-darwin",
    "x86_64-apple-darwin",
)
# GPL-3.0 で配布するアプリに同梱できると確かめたライセンスだけを受け付ける。ここにないライセンスのクレートが
# 依存に入ったら止めるので、条件を確かめてから足す。
ALLOWED_LICENSES = {
    "0BSD", "Apache-2.0", "BSD-2-Clause", "BSD-3-Clause", "BSL-1.0", "CC0-1.0", "ISC", "MIT", "MPL-2.0",
    "Unicode-3.0", "Unicode-DFS-2016", "Unlicense", "Zlib",
}
OPERATORS = {"AND", "OR", "WITH"}
# クレートに入っているライセンスの文書（一番上のフォルダだけを見る）
LICENSE_FILE = re.compile(r"(?i)^(licen[sc]e|copying|notice|unlicense|copyright)")
# ライセンスの文書を同梱していないクレートには、ライセンスの標準の本文を付ける（著作権者は Cargo.toml の authors）。
# 選べるライセンス（OR）のうち、ここにあるものを前から順に使う。
MIT_TEMPLATE = """MIT License

Copyright (c) {holders}

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""
APACHE_TEXT = Path(__file__).resolve().parent / "license-texts/rust-crates/Apache-2.0.txt"


class NoticeError(Exception):
    """The notices cannot be written (missing sources, unknown license, ...)."""


def run_cargo(arguments, offline):
    command = ["cargo", *arguments, "--locked"] + (["--offline"] if offline else [])
    try:
        result = subprocess.run(command, cwd=ROOT, capture_output=True, text=True, check=False)
    except OSError as error:
        raise NoticeError(f"cargo could not be started: {error}") from error
    if result.returncode != 0:
        raise NoticeError(f"`{' '.join(command)}` failed:\n{result.stderr.strip()}")
    return result.stdout


def linked_crates(offline):
    """(name, version) of every crate the peer is built from, for all release targets."""
    crates = set()
    for target in TARGETS:
        output = run_cargo(["tree", "-p", PACKAGE, "-e", "normal,build", "--target", target,
                            "--prefix", "none", "--format", "{p}"], offline)
        for line in output.splitlines():
            match = re.match(r"(\S+) v(\S+)", line)
            if match:
                crates.add((match[1], match[2]))
    return crates


def registry_packages(offline):
    """Package records of the crates.io dependencies, by (name, version)."""
    metadata = json.loads(run_cargo(["metadata", "--format-version", "1"], offline))
    return {(package["name"], package["version"]): package
            for package in metadata["packages"] if package["source"]}


def license_expression(package):
    """The SPDX expression of a crate (old manifests separate alternatives with ``/``)."""
    expression = package.get("license") or ""
    return re.sub(r"\s*/\s*", " OR ", expression).strip()


def license_ids(expression):
    return [token for token in re.findall(r"[A-Za-z0-9][A-Za-z0-9.+-]*", expression)
            if token not in OPERATORS]


def normalized(text):
    """License text with uniform line ends, so the same text from two crates is stored once."""
    lines = [line.rstrip() for line in text.replace("\r\n", "\n").replace("\r", "\n").split("\n")]
    return "\n".join(lines).strip("\n") + "\n"


def shipped_texts(package):
    """(file name, text) of the license documents inside a crate."""
    directory = Path(package["manifest_path"]).parent
    if not directory.is_dir():
        raise NoticeError(f"The source of {package['name']} {package['version']} is not in the cargo registry; "
                          "run `cargo fetch --locked`")
    names = {entry.name for entry in directory.iterdir() if entry.is_file() and LICENSE_FILE.match(entry.name)}
    declared = package.get("license_file")
    if declared and (directory / declared).is_file():
        names.add(declared)
    return [(name, normalized((directory / name).read_text(encoding="utf-8", errors="replace")))
            for name in sorted(names)]


def standard_text(package, expression):
    """The standard text of the first license we hold a text of, for crates that ship none."""
    holders = ", ".join(package.get("authors") or []) or f"The {package['name']} contributors"
    for identifier in license_ids(expression):
        if identifier == "MIT":
            return "MIT (standard text)", normalized(MIT_TEMPLATE.format(holders=holders))
        if identifier == "Apache-2.0":
            return "Apache-2.0 (standard text)", normalized(APACHE_TEXT.read_text(encoding="utf-8"))
    raise NoticeError(f"{package['name']} {package['version']} ships no license text and "
                      f"no standard text is available for {expression}")


def fenced(text):
    """A Markdown code block that shows the text verbatim."""
    longest = max((len(run) for run in re.findall(r"`+", text)), default=0)
    fence = "`" * max(3, longest + 1)
    return f"{fence}text\n{text}{fence}"


def render(crates, packages):
    texts = {}      # digest -> (number, text)
    users = {}      # number -> ["crate version (file)", ...]
    rows = []
    for name, version in sorted(crates):
        package = packages.get((name, version))
        if package is None:
            continue    # ワークスペースの自前のクレート（アプリと同じ GPL-3.0）
        expression = license_expression(package)
        if not expression:
            raise NoticeError(f"{name} {version} declares no license")
        unknown = sorted(set(license_ids(expression)) - ALLOWED_LICENSES)
        if unknown:
            raise NoticeError(f"{name} {version} is under {expression}: {', '.join(unknown)} is not in "
                              "ALLOWED_LICENSES. Check that it can be distributed with a GPL-3.0 application, "
                              "then add it to scripts/rust_licenses.py")
        documents = shipped_texts(package) or [standard_text(package, expression)]
        numbers = []
        for label, text in documents:
            digest = hashlib.sha256(text.encode("utf-8")).hexdigest()
            if digest not in texts:
                texts[digest] = (len(texts) + 1, text)
            number = texts[digest][0]
            users.setdefault(number, []).append(f"{name} {version} ({label})")
            if number not in numbers:
                numbers.append(number)
        rows.append(f"| [{name}](https://crates.io/crates/{name}/{version}) | {version} | {expression} | "
                    + ", ".join(f"T{number}" for number in numbers) + " |")
    if not rows:
        raise NoticeError(f"`cargo tree -p {PACKAGE}` listed no crates")

    lines = [
        "# Rust library licenses (P2P communication process)",
        "",
        "The communication process of P2P games (`shogiboardq-peer`) is written in Rust and is built from the",
        "crates below, which are linked into the executable. Each crate is distributed under its own license.",
        "The table lists the crates for all supported systems (Linux, Windows and macOS); a build for one",
        "system contains a subset. Where a crate offers a choice of licenses, any of them may be used.",
        "",
        "The source code of each crate is available from crates.io at the linked page (the exact versions are",
        "pinned in `Cargo.lock` of the ShogiBoardQ source). \"T\" numbers refer to the license texts at the end",
        "of this document.",
        "",
        "The executable also contains the Rust standard library (https://github.com/rust-lang/rust), which is",
        "distributed under MIT OR Apache-2.0. Its copyright notices are in the COPYRIGHT file of that repository.",
        "",
        f"## Crates ({len(rows)})",
        "",
        "| Crate | Version | License | License texts |",
        "|---|---|---|---|",
        *rows,
        "",
        "## License texts",
    ]
    for number, text in sorted(texts.values()):
        lines += ["", f"### T{number}", "", "Used by: " + ", ".join(users[number]), "", fenced(text)]
    return "\n".join(lines) + "\n"


def generate(offline=False):
    return render(linked_crates(offline), registry_packages(offline))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--check", action="store_true",
                        help="do not write; exit 1 when the document differs from what Cargo.lock gives")
    parser.add_argument("--offline", action="store_true", help="do not let cargo use the network")
    args = parser.parse_args()
    try:
        document = generate(args.offline)
    except NoticeError as error:
        parser.exit(2, f"Rust library notices failed: {error}\n")
    if args.check:
        current = args.output.read_text(encoding="utf-8") if args.output.is_file() else ""
        if current != document:
            parser.exit(1, f"{args.output} is out of date. Run `python3 scripts/rust_licenses.py` and commit it.\n")
        print(f"{args.output} is up to date")
        return
    args.output.write_text(document, encoding="utf-8")
    print(f"Wrote the notices of {document.count('| [')} crates to {args.output}")


if __name__ == "__main__":
    sys.exit(main())
