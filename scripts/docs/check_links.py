#!/usr/bin/env python3
"""Check that local links and images in docs pages exist.

usage: check_links.py guide/<page>.html [...]   (checks the ja, en, zh-cn and zh-tw versions that exist)
       check_links.py --all                      (every HTML page under docs/)

Absolute URLs (http:, https:, mailto:) and in-page anchors are not checked. Exits with 1 when a target is missing.
"""
import os
import re
import sys

from guide_common import DOCS, LANGS


def pages(args):
    if args == ["--all"]:
        for root, _dirs, files in os.walk(DOCS):
            for name in files:
                if name.endswith(".html"):
                    yield os.path.join(root, name)
        return
    for page in args:
        for _code, directory, *_ in LANGS:
            path = os.path.join(DOCS, directory, page)
            if os.path.exists(path):
                yield path


missing = 0
for path in pages(sys.argv[1:]):
    text = open(path, encoding="utf-8").read()
    bad = sorted({u for u in re.findall(r'(?:href|src)="([^"#:?]+)(?:[?#][^"]*)?"', text)
                  if u and not os.path.exists(os.path.normpath(os.path.join(os.path.dirname(path), u)))})
    sections = (text.count("<section"), text.count("</section>"))
    print(f"{os.path.relpath(path, DOCS)}: {'OK' if not bad else 'missing ' + ', '.join(bad)}"
          + ("" if sections[0] == sections[1] else f"  (unbalanced <section>: {sections[0]}/{sections[1]})"))
    missing += len(bad) + (sections[0] != sections[1])
sys.exit(1 if missing else 0)
