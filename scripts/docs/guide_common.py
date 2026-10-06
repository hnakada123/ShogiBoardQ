"""Shared helpers for translated guide pages (language menu, hreflang, ja/en conversion)."""
import os
import re

# リポジトリの docs/（このファイルは scripts/docs/ にある）
DOCS = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))), "docs")
SITE = "https://hnakada123.github.io/ShogiBoardQ/"
LANGS = [  # (code, dir, html lang, hreflang, og locale, label)
    ("ja", "", "ja", "ja", "ja_JP", "日本語"),
    ("en", "en/", "en", "en", "en_US", "English"),
    ("zh-cn", "zh-cn/", "zh-Hans", "zh-Hans", "zh_CN", "简体中文"),
    ("zh-tw", "zh-tw/", "zh-Hant", "zh-Hant", "zh_TW", "繁體中文"),
]
GLOBE = ('<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" '
         'stroke-linecap="round" aria-hidden="true"><circle cx="12" cy="12" r="9"/>'
         '<path d="M3 12h18M12 3c2.5 2.7 3.8 5.7 3.8 9s-1.3 6.3-3.8 9c-2.5-2.7-3.8-5.7-3.8-9S9.5 5.7 12 3Z"/></svg>')
MENU_SCRIPT = """<script>
document.addEventListener('click', function (event) {
    document.querySelectorAll('.lang-menu[open]').forEach(function (menu) {
        if (!menu.contains(event.target)) menu.removeAttribute('open');
    });
});
document.addEventListener('keydown', function (event) {
    if (event.key !== 'Escape') return;
    document.querySelectorAll('.lang-menu[open]').forEach(function (menu) {
        menu.removeAttribute('open');
        menu.querySelector('summary').focus();
    });
});
</script>"""


def meta(code):
    return next(l for l in LANGS if l[0] == code)


def alternates(page, indent):
    lines = [f'{indent}<link rel="alternate" hreflang="{h}" href="{SITE}{d}{page}">' for _, d, _, h, _, _ in LANGS]
    lines.append(f'{indent}<link rel="alternate" hreflang="x-default" href="{SITE}{page}">')
    return "\n".join(lines)


def lang_menu(page, current, aria, indent):
    depth = "../" if current == "ja" else "../../"
    label = meta(current)[5]
    out = [f'{indent}<details class="lang-menu">',
           f'{indent}    <summary aria-label="{aria}">{GLOBE}<span>{label}</span></summary>',
           f'{indent}    <ul>']
    filename = page.split("/")[-1]
    for code, d, lang, hreflang, _, name in LANGS:
        href = filename if code == current else f"{depth}{d}{page}"
        cur = ' aria-current="page"' if code == current else ""
        out.append(f'{indent}        <li><a href="{href}" lang="{lang}" hreflang="{hreflang}"{cur}>{name}</a></li>')
    out += [f'{indent}    </ul>', f'{indent}</details>']
    return "\n".join(out)


def update_existing(page, code, aria):
    """Give an existing ja/en page the 4-language menu and hreflang set."""
    path = f"{DOCS}/{meta(code)[1]}{page}"
    t = open(path, encoding="utf-8").read()
    if 'class="lang-menu"' in t:
        return
    t = re.sub(r'\s*<link (?:rel="alternate" hreflang="[^"]+" href="[^"]+"|href="[^"]+" hreflang="[^"]+" rel="alternate")/?>', "", t)
    indent = "    " if code == "ja" else ""
    t = t.replace("</head>", alternates(page, indent) + "\n</head>", 1)
    t, n = re.subn(r'[ \t]*<a [^>]*class="language-switch"[^>]*>[^<]*</a>',
                   lang_menu(page, code, aria, "        " if code == "ja" else ""), t)
    assert n == 1, (path, n)
    t = t.replace("</body>", MENU_SCRIPT + "\n\n</body>", 1)
    open(path, "w", encoding="utf-8").write(t)
