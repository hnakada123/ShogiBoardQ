#!/usr/bin/env python3
"""Generate docs/guide/index.html and its en / zh-cn / zh-tw translations (the user guide contents).

各機能のカードの見出しと説明は、ホーム（gen_home.py）の機能一覧と同じ文言を使う。
翻訳版が無いページは英語版へリンクし、見出しに（英文）と添える。
"""
import os

from gen_home import FEATURE_GROUPS, T as HOME
from guide_common import DOCS, MENU_SCRIPT, alternates, lang_menu, meta

ORDER = ["ja", "en", "zh-cn", "zh-tw"]
# 目次の並び（導入から始め、最後にビルド手順）
GROUP_ORDER = ["setup", "play", "records", "analysis", "tsume", "ai"]
BUILD_PAGES = ["linux-build-and-release", "windows-build-and-release", "macos-build-and-release"]

T = {
    "ja": {
        "title": "利用ガイド - ShogiBoardQ",
        "desc": "ShogiBoardQの利用ガイド。導入・画面設定、対局、棋譜・局面の操作、研究・解析、詰将棋、AI連携、"
                "ソースからのビルドの説明を目的別に探せます。",
        "h1": "利用ガイド",
        "subtitle": "目的に合わせて、機能と操作方法を探せます",
        "home": "ホーム", "menu": "メニュー", "language": "表示言語",
        "back": "&larr; ホームに戻る",
        "build_title": "ソースからのビルド",
        "build": [
            ("Linux ビルド・リリース手順", "ソースからのビルドと、AppImage と配布 ZIP の作成・公開の手順"),
            ("Windows ビルド・リリース手順", "MSVC でのビルドと、配布 ZIP の作成・公開の手順"),
            ("macOS ビルド・リリース手順", "ビルドと、DMG を入れた配布 ZIP の作成・公開の手順"),
        ],
        "english": "",
    },
    "en": {
        "title": "User Guide - ShogiBoardQ",
        "desc": "The ShogiBoardQ user guide: setup and appearance, playing games, game records and positions, "
                "study and analysis, tsume shogi, AI integration, and building from source, organized by task.",
        "h1": "User Guide",
        "subtitle": "Find the features and instructions you need",
        "home": "Home", "menu": "Menu", "language": "Language",
        "back": "&larr; Back to Home",
        "build_title": "Build from Source",
        "build": [
            ("Linux Build and Release", "Build from source, and create and publish the AppImage and release ZIP"),
            ("Windows Build and Release", "Build with MSVC, and create and publish the release ZIP"),
            ("macOS Build and Release", "Build, and create and publish the release ZIP that contains the DMG"),
        ],
        "english": "",
    },
    "zh-cn": {
        "title": "使用指南 - ShogiBoardQ",
        "desc": "ShogiBoardQ 使用指南：按用途查找界面与设置、对局、棋谱与局面、研究与分析、诘棋、AI 集成以及从源代码构建的说明。",
        "h1": "使用指南",
        "subtitle": "按用途查找功能和操作方法",
        "home": "首页", "menu": "菜单", "language": "界面语言",
        "back": "&larr; 返回首页",
        "build_title": "从源代码构建",
        "build": [
            ("Linux 构建与发布", "从源代码构建，以及制作 AppImage 和发布包（ZIP）并发布的步骤"),
            ("Windows 构建与发布", "使用 MSVC 构建，以及制作发布包（ZIP）并发布的步骤"),
            ("macOS 构建与发布", "构建，以及制作包含 DMG 的发布包（ZIP）并发布的步骤"),
        ],
        "english": "（英文）",
    },
    "zh-tw": {
        "title": "使用指南 - ShogiBoardQ",
        "desc": "ShogiBoardQ 使用指南：依用途查找介面與設定、對局、棋譜與局面、研究與分析、詰棋、AI 整合以及從原始碼建置的說明。",
        "h1": "使用指南",
        "subtitle": "依用途查找功能與操作方式",
        "home": "首頁", "menu": "選單", "language": "介面語言",
        "back": "&larr; 返回首頁",
        "build_title": "從原始碼建置",
        "build": [
            ("Linux 建置與發行", "從原始碼建置，以及製作 AppImage 與發行套件（ZIP）並發行的步驟"),
            ("Windows 建置與發行", "使用 MSVC 建置，以及製作發行套件（ZIP）並發行的步驟"),
            ("macOS 建置與發行", "建置，以及製作含 DMG 的發行套件（ZIP）並發行的步驟"),
        ],
        "english": "（英文）",
    },
}


def page(code):
    t = T[code]
    home = HOME[code]
    _, d, lang, _, _, _ = meta(code)
    css = "../css/style.css" if code == "ja" else "../../css/style.css"
    groups = dict(FEATURE_GROUPS)

    def link(slug, title):
        # 翻訳版が無いページは英語版へ（# 以降はページ内の節）
        page, sep, frag = slug.partition("#")
        if os.path.exists(os.path.join(DOCS, d, "guide", page + ".html")):
            return page + ".html" + sep + frag, title
        return "../../en/guide/" + page + ".html" + sep + frag, title + t["english"]

    def card(slug, title, desc):
        href, title = link(slug, title)
        return f"""            <a href="{href}" class="feature-card-link">
                <div class="feature-card">
                    <h3>{title}</h3>
                    <p>{desc}</p>
                    <span class="feature-card-more">{home['more']} &rarr;</span>
                </div>
            </a>
"""

    sections = [(g, home["groups"][g], [card(slug, *home["cards"][slug]) for _, slug in groups[g]])
                for g in GROUP_ORDER]
    sections.append(("build", t["build_title"],
                     [card(slug, title, desc) for slug, (title, desc) in zip(BUILD_PAGES, t["build"])]))

    nav = "".join(f'            <li><a href="#{key}">{title}</a></li>\n' for key, title, _ in sections)
    body = ""
    for i, (key, title, cards) in enumerate(sections):
        cls = "section section--alt" if i % 2 == 0 else "section"
        body += f"""<section id="{key}" class="{cls}">
    <div class="section-inner">
        <h2 class="section-title">{title}</h2>
        <div class="features-grid">
{"".join(cards)}        </div>
    </div>
</section>
"""
    return f"""<!DOCTYPE html>
<html lang="{lang}">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>{t['title']}</title>
    <meta name="description" content="{t['desc']}">
{alternates("guide/", "    ")}
    <link rel="canonical" href="https://hnakada123.github.io/ShogiBoardQ/{d}guide/">
    <link rel="stylesheet" href="{css}">
</head>
<body>
<nav class="site-nav">
    <div class="nav-inner">
        <a href="../index.html" class="nav-logo">ShogiBoardQ</a>
{lang_menu("guide/index.html", code, t['language'], "        ")}
        <button class="nav-toggle" aria-label="{t['menu']}" aria-controls="guide-nav" aria-expanded="false" onclick="const menu = document.getElementById('guide-nav'); this.setAttribute('aria-expanded', menu.classList.toggle('open'));">
            <span></span><span></span><span></span>
        </button>
        <ul class="nav-links" id="guide-nav" onclick="if (event.target.closest('a')) {{ this.classList.remove('open'); document.querySelector('.nav-toggle').setAttribute('aria-expanded', 'false'); }}">
            <li><a href="../index.html">{t['home']}</a></li>
{nav}        </ul>
    </div>
</nav>
<main>
<section class="hero hero--sub">
    <div class="hero-content">
        <h1>{t['h1']}</h1>
        <p class="hero-subtitle">{t['subtitle']}</p>
    </div>
</section>
{body}</main>
<div class="back-link-section">
    <a href="../index.html" class="back-link">{t['back']}</a>
</div>
<footer class="site-footer">
    <p>&copy; 2025 ShogiBoardQ</p>
</footer>
{MENU_SCRIPT}
</body>
</html>
"""


if __name__ == "__main__":
    for code in ORDER:
        path = os.path.join(DOCS, meta(code)[1], "guide", "index.html")
        with open(path, "w", encoding="utf-8") as f:
            f.write(page(code))
        print("wrote", path)
