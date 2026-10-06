#!/usr/bin/env python3
"""Add the zh-cn / zh-tw versions of a guide page to docs/sitemap.xml.

usage: sitemap_add_zh.py guide/<page>.html

Gives the ja and en blocks of the page the ja / en / zh-Hans / zh-Hant / x-default alternates and appends
zh-cn and zh-tw blocks before </urlset>. Run it once, after the zh pages exist.
"""
import os
import sys
import xml.dom.minidom

from guide_common import DOCS, SITE

page = sys.argv[1]
path = os.path.join(DOCS, "sitemap.xml")
text = open(path, encoding="utf-8").read()
old = (f'    <xhtml:link rel="alternate" hreflang="ja" href="{SITE}{page}" />\n'
       f'    <xhtml:link rel="alternate" hreflang="en" href="{SITE}en/{page}" />\n'
       f'    <xhtml:link rel="alternate" hreflang="x-default" href="{SITE}{page}" />\n')
new = (f'    <xhtml:link rel="alternate" hreflang="ja" href="{SITE}{page}" />\n'
       f'    <xhtml:link rel="alternate" hreflang="en" href="{SITE}en/{page}" />\n'
       f'    <xhtml:link rel="alternate" hreflang="zh-Hans" href="{SITE}zh-cn/{page}" />\n'
       f'    <xhtml:link rel="alternate" hreflang="zh-Hant" href="{SITE}zh-tw/{page}" />\n'
       f'    <xhtml:link rel="alternate" hreflang="x-default" href="{SITE}{page}" />\n')
if text.count(old) != 2:
    sys.exit(f"expected the ja and en blocks of {page} with ja/en alternates only (found {text.count(old)})")
text = text.replace(old, new)
blocks = "".join(f"  <url>\n    <loc>{SITE}{d}/{page}</loc>\n{new}    <changefreq>monthly</changefreq>\n"
                 f"    <priority>0.8</priority>\n  </url>\n" for d in ("zh-cn", "zh-tw"))
text = text.replace("</urlset>", blocks + "</urlset>")
xml.dom.minidom.parseString(text.encode("utf-8"))
open(path, "w", encoding="utf-8").write(text)
print("updated", path)
