#!/usr/bin/env python3
"""Write docs/{,en/,zh-cn/,zh-tw/}guide/linux-build-and-release.html and docs/dev/linux-build-and-release.md.

本文は texts_linux_build.py に言語ごとのブロック列で置く（4言語のページと、日本語の本文から作る
開発者向けの md は、どちらもこのスクリプトの出力なので直接編集しない）。
使い方: python3 scripts/docs/build_guide/gen_linux_build.py
インライン記法は `code`・**強調**・[文字](URL)。
ブロック: ("h2", id, 見出し) ("h3", id, 見出し) ("p", 文) ("note", 文) ("code", テキスト)
("ul", [文…]) ("ol", [文…]) ("table", [見出し…], [[セル…]…][, "labels"]) ("toc", [(id, 見出し)…])
"""
import html
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(HERE))  # scripts/docs（guide_common）
sys.path.insert(0, HERE)
from guide_common import DOCS, SITE, MENU_SCRIPT, alternates, lang_menu, meta  # noqa: E402
from texts_linux_build import T  # noqa: E402

PAGE = "guide/linux-build-and-release.html"


def inline(s):
    codes = []

    def keep(match):
        codes.append(f"<code>{match.group(1)}</code>")
        return f"\x00{len(codes) - 1}\x00"

    s = html.escape(s, quote=False)
    s = re.sub(r"`([^`]+)`", keep, s)
    s = re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", s)
    s = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r'<a href="\2">\1</a>', s)
    return re.sub(r"\x00(\d+)\x00", lambda m: codes[int(m.group(1))], s)


def body(blocks):
    out = []
    w = out.append
    level = 0  # 開いている <section> の深さ（h2=1, h3=2）

    def close(to):
        nonlocal level
        while level > to:
            w("</section>")
            level -= 1

    for block in blocks:
        kind = block[0]
        if kind == "h2":
            close(0)
            w(f'<section id="{block[1]}" class="level2">\n<h2>{inline(block[2])}</h2>')
            level = 1
        elif kind == "h3":
            close(1)
            w(f'<section id="{block[1]}" class="level3">\n<h3>{inline(block[2])}</h3>')
            level = 2
        elif kind == "p":
            w(f"<p>{inline(block[1])}</p>")
        elif kind == "note":
            w(f"<blockquote>\n<p>{inline(block[1])}</p>\n</blockquote>")
        elif kind == "code":
            w(f"<pre><code>{html.escape(block[1].strip(chr(10)), quote=False)}</code></pre>")
        elif kind in ("ul", "ol"):
            items = "\n".join(f"<li>{inline(item)}</li>" for item in block[1])
            w(f"<{kind}>\n{items}\n</{kind}>")
        elif kind == "toc":
            items = "\n".join(f'<li><a href="#{i}">{inline(t)}</a></li>' for i, t in block[1])
            w(f"<ol>\n{items}\n</ol>")
        elif kind == "table":
            head = "".join(f"<th>{inline(h)}</th>" for h in block[1])
            rows = "\n".join("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in row) + "</tr>" for row in block[2])
            cls = "info-table info-table--labels" if len(block) > 3 and block[3] == "labels" else "info-table"
            w(f'<div class="table-scroll">\n<table class="{cls}">\n'
              f"<thead>\n<tr>{head}</tr>\n</thead>\n<tbody>\n{rows}\n</tbody>\n</table>\n</div>")
        else:
            raise ValueError(kind)
    close(0)
    return "\n".join(out)


def page(code):
    t = T[code]
    _, d, lang, _, locale, _ = meta(code)
    up = "../" if code == "ja" else "../../"
    url = f"{SITE}{d}{PAGE}"
    e = lambda s: html.escape(s, quote=True)  # noqa: E731
    title = f"{t['title']} - ShogiBoardQ"
    nav = "\n".join(f'            <li><a href="{href}">{e(label)}</a></li>' for label, href in t["nav"])
    badges = "\n".join(f'            <span class="badge">{e(b)}</span>' for b in t["badges"])
    return f"""<!DOCTYPE html>
<html lang="{lang}">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{e(title)}</title>
    <meta name="description" content="{e(t['description'])}">
    <link rel="canonical" href="{url}">
{alternates(PAGE, "    ")}
    <meta property="og:type" content="article">
    <meta property="og:url" content="{url}">
    <meta property="og:title" content="{e(title)}">
    <meta property="og:description" content="{e(t['summary'])}">
    <meta property="og:image" content="{SITE}images/og-image.png?v=20261005">
    <meta property="og:locale" content="{locale}">
    <meta property="og:site_name" content="ShogiBoardQ">
    <meta name="twitter:card" content="summary_large_image">
    <meta name="twitter:title" content="{e(title)}">
    <meta name="twitter:description" content="{e(t['summary'])}">
    <meta name="twitter:image" content="{SITE}images/og-image.png?v=20261005">
    <link rel="stylesheet" href="{up}css/style.css">
    <link rel="stylesheet" href="{up}css/build-guide.css">
</head>
<body>

<nav class="site-nav">
    <div class="nav-inner">
        <a href="../index.html" class="nav-logo">ShogiBoardQ</a>
{lang_menu(PAGE, code, t['lang_aria'], "        ")}
        <button class="nav-toggle" aria-label="{e(t['menu_aria'])}" aria-controls="guide-nav" aria-expanded="false" onclick="const menu = document.getElementById('guide-nav'); this.setAttribute('aria-expanded', menu.classList.toggle('open'));">
            <span></span><span></span><span></span>
        </button>
        <ul class="nav-links" id="guide-nav" onclick="if (event.target.closest('a')) {{ this.classList.remove('open'); document.querySelector('.nav-toggle').setAttribute('aria-expanded', 'false'); }}">
{nav}
        </ul>
    </div>
</nav>

<main class="build-guide">
<section class="hero hero--sub">
    <div class="hero-content">
        <h1>{e(t['title'])}</h1>
        <p class="hero-subtitle">{e(t['subtitle'])}</p>
        <div class="hero-badges">
{badges}
        </div>
    </div>
</section>

<div class="section">
<article class="section-inner build-document" aria-label="{e(t['title'])}">
{body(t['blocks'])}
</article>
</div>
</main>

<div class="back-link-section">
    <a href="{t['back'][1]}" class="back-link">&larr; {e(t['back'][0])}</a>
</div>
<footer class="site-footer">
    <p>&copy; 2025 ShogiBoardQ</p>
</footer>

{MENU_SCRIPT}

</body>
</html>
"""


MD = os.path.join(os.path.dirname(DOCS), "docs", "dev", "linux-build-and-release.md")


def markdown(code="ja"):
    """Developer copy of the page (docs/dev) in Markdown, from the same blocks."""
    t = T[code]
    out = [f"# {t['title']}", "",
           "<!-- scripts/docs/build_guide/gen_linux_build.py で生成。本文は texts_linux_build.py を編集する。 -->",
           "", f"公開ページ: {SITE}{PAGE}", ""]
    w = out.append
    for block in t["blocks"]:
        kind = block[0]
        if kind in ("h2", "h3"):
            # 公開ページと同じ id でページ内リンクできるようにする
            w(f'<a id="{block[1]}"></a>')
            w("")
            w(("## " if kind == "h2" else "### ") + block[2])
        elif kind == "p":
            w(block[1])
        elif kind == "note":
            w("> " + block[1])
        elif kind == "code":
            w("```")
            w(block[1].strip("\n"))
            w("```")
        elif kind in ("ul", "ol"):
            for index, item in enumerate(block[1], 1):
                w((f"{index}. " if kind == "ol" else "- ") + item)
        elif kind == "toc":
            for index, (anchor, title) in enumerate(block[1], 1):
                w(f"{index}. [{title}](#{anchor})")
        elif kind == "table":
            w("| " + " | ".join(block[1]) + " |")
            w("|" + "---|" * len(block[1]))
            for row in block[2]:
                w("| " + " | ".join(row) + " |")
        else:
            raise ValueError(kind)
        w("")
    return "\n".join(out).rstrip("\n") + "\n"


if __name__ == "__main__":
    for code in T:
        path = os.path.join(DOCS, meta(code)[1], PAGE)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write(page(code))
        print("wrote", path)
    with open(MD, "w", encoding="utf-8") as f:
        f.write(markdown())
    print("wrote", MD)
