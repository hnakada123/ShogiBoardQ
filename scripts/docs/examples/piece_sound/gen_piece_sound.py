#!/usr/bin/env python3
"""Write docs/{,en/,zh-cn/,zh-tw/}guide/piece-sound.html from one template.

新しいガイドページを作るときの見本: 本文は texts.py に言語ごとに置き（[[表示|リンク先]] はリンクになる）、
画像は docs/images/{,en/,zh-cn/,zh-tw/}<ページ名>/ から読み、幅・高さを img に書き込む。
"""
import html
import os
import re
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.dirname(os.path.dirname(HERE)))  # scripts/docs（guide_common）
sys.path.insert(0, HERE)  # texts.py
from guide_common import DOCS, SITE, MENU_SCRIPT, alternates, lang_menu, meta
from texts import T

PAGE = "guide/piece-sound.html"
IMAGES = {"ja": "", "en": "en/", "zh-cn": "zh-cn/", "zh-tw": "zh-tw/"}
ANCHORS = ["toggle", "settings", "when"]


def png_size(path):
    with open(path, "rb") as f:
        f.read(16)
        return struct.unpack(">II", f.read(8))


def page(code):
    t = T[code]
    _, d, lang, _, locale, _ = meta(code)
    up = "../" if code == "ja" else "../../"
    img = f"{up}images/{IMAGES[code]}piece-sound/"
    url = f"{SITE}{d}{PAGE}"
    og_img = f"{SITE}images/{IMAGES[code]}piece-sound/piece-sound-settings.png"
    guide = "index.html"
    colon = ": " if code == "en" else "："
    e = html.escape
    o = []
    w = o.append

    def text(s):
        return e(s, quote=False)

    def rich(s):
        return re.sub(r"\[\[([^|\]]+)\|([^\]]+)\]\]", r'<a href="\2">\1</a>', text(s))

    def para(s):
        w(f'                <p class="guide-step-text">{rich(s)}</p>\n')

    def shot(name):
        alt, cap = t["shots"][name]
        width, height = png_size(f"{DOCS}/images/{IMAGES[code]}piece-sound/{name}.png")
        w('                <div class="screenshot-item screenshot-item--compact">\n'
          f'                    <a href="{img}{name}.png"><img src="{img}{name}.png" alt="{e(alt)}" width="{width}" height="{height}" loading="lazy"></a>\n'
          f'                    <p class="screenshot-caption">{text(cap)}</p>\n                </div>\n')

    def step_open(n, title):
        w(f"""
            <div class="guide-step">
                <div class="guide-step-header">
                    <span class="guide-step-number">{n}</span>
                    <span class="guide-step-title">{text(title)}</span>
                </div>
""")

    def step(n, key, shots=()):
        title, paras = t[key]
        step_open(n, title)
        for p in paras:
            para(p)
        for name in shots:
            shot(name)
        w("            </div>\n")

    def section(index, alt, key):
        title, desc = t[key]
        cls = "section section--alt" if alt else "section"
        w(f"""
<!-- Section {index + 1} -->
<section id="{ANCHORS[index]}" class="{cls}">
    <div class="section-inner">
        <h2 class="section-title">{text(title)}</h2>
        <p class="section-desc">{text(desc)}</p>

        <div class="screenshot-gallery">
""")

    def end_section():
        w("\n        </div>\n    </div>\n</section>\n")

    badges = "\n".join(f'            <span class="badge">{text(b)}</span>' for b in t["badges"])
    note = f'\n        <p class="screenshot-note">{text(t["note"])}</p>' if t["note"] else ""
    nav = "\n".join(f'            <li><a href="#{a}">{text(n)}</a></li>' for a, n in zip(ANCHORS, t["nav"][2:]))
    w(f"""<!DOCTYPE html>
<html lang="{lang}">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{e(t['title'])}</title>
    <meta name="description" content="{e(t['desc'])}">
    <link rel="canonical" href="{url}">
    <!-- Open Graph -->
    <meta property="og:type" content="article">
    <meta property="og:url" content="{url}">
    <meta property="og:title" content="{e(t['title'])}">
    <meta property="og:description" content="{e(t['og_desc'])}">
    <meta property="og:image" content="{og_img}">
    <meta property="og:locale" content="{locale}">
    <meta property="og:site_name" content="ShogiBoardQ">
    <!-- Twitter Card -->
    <meta name="twitter:card" content="summary_large_image">
    <meta name="twitter:title" content="{e(t['title'])}">
    <meta name="twitter:description" content="{e(t['og_desc'])}">
    <meta name="twitter:image" content="{og_img}">
    <link rel="stylesheet" href="{up}css/style.css">
{alternates(PAGE, '    ')}
</head>
<body>

<!-- Navigation -->
<nav class="site-nav">
    <div class="nav-inner">
        <a href="../index.html" class="nav-logo">ShogiBoardQ</a>
{lang_menu(PAGE, code, t['lang'], '        ')}
        <button class="nav-toggle" aria-label="{t['menu']}" aria-controls="guide-nav" aria-expanded="false" onclick="const menu = document.getElementById('guide-nav'); this.setAttribute('aria-expanded', menu.classList.toggle('open'));">
            <span></span><span></span><span></span>
        </button>
        <ul class="nav-links" id="guide-nav" onclick="if (event.target.closest('a')) {{ this.classList.remove('open'); document.querySelector('.nav-toggle').setAttribute('aria-expanded', 'false'); }}">
            <li><a href="../index.html">{text(t['nav'][0])}</a></li>
            <li><a href="{guide}">{text(t['nav'][1])}</a></li>
{nav}
            <li><a href="https://github.com/hnakada123/ShogiBoardQ">GitHub</a></li>
        </ul>
    </div>
</nav>

<main>
<!-- Hero -->
<section class="hero hero--sub">
    <div class="hero-content">
        <h1>{text(t['h1'])}</h1>
        <p class="hero-subtitle">{text(t['subtitle'])}</p>
        <div class="hero-badges">
{badges}
        </div>{note}
    </div>
</section>
""")
    section(0, True, "s_toggle")
    step(1, "toggle", ["settings-menu"])
    step(2, "saved")
    end_section()

    section(1, False, "s_settings")
    step(1, "open", ["piece-sound-settings"])
    title, intro, items = t["sliders"]
    step_open(2, title)
    w('                <ul class="guide-step-text">\n'
      + "".join(f"                    <li><strong>{text(a)}</strong>{colon}{text(b)}</li>\n" for a, b in items)
      + "                </ul>\n")
    para(intro)
    w("            </div>\n")
    step(3, "finish")
    end_section()

    section(2, True, "s_when")
    w('\n            <div class="guide-step">\n                <table class="info-table">\n                    <thead>\n'
      "                        <tr>" + "".join(f'<th scope="col">{text(h)}</th>' for h in t["table_head"]) + "</tr>\n"
      "                    </thead>\n                    <tbody>\n")
    for label, plays, notes in t["rows"]:
        w(f"                        <tr><td><strong>{rich(label)}</strong></td><td>{text(t['yes'] if plays else t['no'])}</td><td>{rich(notes)}</td></tr>\n")
    w("                    </tbody>\n                </table>\n")
    para(t["after"])
    w("            </div>\n")
    end_section()

    w(f"""</main>

<!-- Back Link -->
<div class="back-link-section">
    <a href="{guide}" class="back-link">{text(t['back'])}</a>
</div>

<!-- Footer -->
<footer class="site-footer">
    <p>&copy; 2025 ShogiBoardQ</p>
</footer>

{MENU_SCRIPT}

</body>
</html>
""")
    return "".join(o)


for code in T:
    path = f"{DOCS}/{meta(code)[1]}{PAGE}"
    os.makedirs(os.path.dirname(path), exist_ok=True)
    open(path, "w", encoding="utf-8").write(page(code))
    print("wrote", path)
