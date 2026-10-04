#!/usr/bin/env python3
"""現標準駒の輪郭に、Noto Serif CJK JPの欧文をパスとして配置する。"""
import argparse
import os
import re
from pathlib import Path
import xml.etree.ElementTree as ET

os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
from PySide6.QtGui import QGuiApplication, QRawFont, QFont, QTransform

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
NS = 'http://www.w3.org/2000/svg'
ET.register_namespace('', NS)
TAG = lambda name: f'{{{NS}}}{name}'
STYLES = {
    'sei': ('SemiBold', 0.98),
    'rin': ('Medium', 0.76),
    'sumi': ('Black', 1.02),
}
# 駒の縮尺を適用する前の45×45座標で統一する寸法。
# 文字と余白にも標準駒と同じ縮尺を適用し、五角形の大きさに比例させる。
LETTER_HEIGHT = 22.0
BOTTOM_GAP = 6.5
RIGHT_SHIFT = 0.3
SIDE_GAP = 1.8
ROLES = {'ou': 'K', 'gyoku': 'K', 'hi': 'R', 'kaku': 'B', 'kin': 'G', 'gin': 'S',
         'kei': 'N', 'kyou': 'L', 'fu': 'P', 'ryuu': 'R', 'uma': 'B',
         'narigin': 'S', 'narikei': 'N', 'narikyou': 'L', 'to': 'P'}
PROMOTED = {'ryuu', 'uma', 'narigin', 'narikei', 'narikyou', 'to'}


def piece_geometry(template):
    root = ET.parse(template).getroot()
    polygon = next(root.iter(TAG('polygon')))
    parents = {child: parent for parent in root.iter() for child in parent}
    scale, node = 1.0, polygon
    while node in parents:
        value = node.get('transform')
        if value:
            match = re.fullmatch(r'matrix\(([^)]+)\)', value)
            assert match, value
            a, b, c, d, _, _ = map(float, re.split(r'[ ,]+', match[1]))
            assert b == c == 0 and a == d and a > 0, value
            scale *= a
        node = parents[node]
    points = [tuple(map(float, pair.split(','))) for pair in polygon.get('points').split()]
    return scale, points, float(polygon.get('stroke-width'))


def available_width(points, top, bottom, center, inset):
    """文字の上下端と輪郭の角で横幅を調べ、字形全体に斜辺との余白を残す。"""
    levels = [top, bottom] + [y for _, y in points if top < y < bottom]
    width = float('inf')
    for y in levels:
        intersections = []
        for (x1, y1), (x2, y2) in zip(points, points[1:] + points[:1]):
            if y1 != y2 and min(y1, y2) <= y <= max(y1, y2):
                intersections.append(x1 + (x2 - x1) * (y - y1) / (y2 - y1))
        assert len(intersections) >= 2
        width = min(width, 2 * (center - min(intersections) - inset),
                    2 * (max(intersections) - center - inset))
    assert width > 0
    return width


def path_data(path):
    commands = []
    index = 0
    while index < path.elementCount():
        point = path.elementAt(index)
        if point.isMoveTo():
            commands.append(f'M{point.x:.4f},{point.y:.4f}')
        elif point.isLineTo():
            commands.append(f'L{point.x:.4f},{point.y:.4f}')
        elif point.type == path.ElementType.CurveToElement:
            control, end = path.elementAt(index + 1), path.elementAt(index + 2)
            commands.append(f'C{point.x:.4f},{point.y:.4f} {control.x:.4f},{control.y:.4f} {end.x:.4f},{end.y:.4f}')
            index += 2
        index += 1
    return ' '.join(commands)


SURFACES = {
    'wood': ('#f7e9c9', '#9b825c'),
    'paper': ('#fffef9', '#a9aaa0'),
    'slate': ('#f4eada', '#b5a58b'),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True,
                        help='駒セットの出力先（resources/images/pieces など）')
    parser.add_argument('--font-dir', type=Path, default=Path('/usr/share/fonts/noto-cjk'),
                        help='NotoSerifCJK-{SemiBold,Medium,Black}.ttc の格納先')
    args = parser.parse_args()
    app = QGuiApplication([])
    resources = ET.parse(REPO / 'resources/shogiboardq.qrc').getroot()
    templates = {node.get('alias'): REPO / 'resources' / node.text
                 for node in resources.iter('file') if node.get('alias', '').startswith('pieces/')}
    geometry = {role: piece_geometry(templates[f'pieces/Sente_{name}45.svg'])
                for name, role in ROLES.items() if name not in PROMOTED and name != 'gyoku'}
    for style, (weight, width_factor) in STYLES.items():
        font_path = args.font_dir / f'NotoSerifCJK-{weight}.ttc'
        font = QRawFont(str(font_path), 1000, QFont.HintingPreference.PreferNoHinting)
        if not font.isValid():
            raise RuntimeError(f'フォントを読み込めません: {font_path}')
        glyphs = {}
        for role in sorted(set(ROLES.values())):
            glyph = font.pathForGlyph(font.glyphIndexesForString(role)[0])
            bounds = glyph.boundingRect()
            _, points, stroke_width = geometry[role]
            # 実際の字形の上端・下端で合わせ、標準駒の変換で文字も比例縮小する。
            center = 22.5 + RIGHT_SHIFT
            bottom = max(y for _, y in points) - BOTTOM_GAP
            sy = LETTER_HEIGHT / bounds.height()
            width = available_width(points, bottom - LETTER_HEIGHT, bottom, center,
                                    SIDE_GAP + stroke_width / 2)
            sx = min(sy * width_factor, width / bounds.width())
            transform = QTransform()
            transform.translate(center - bounds.center().x() * sx, bottom - bounds.bottom() * sy)
            transform.scale(sx, sy)
            glyphs[role] = path_data(transform.map(glyph))
        for surface, (face, border) in SURFACES.items():
            destination = args.output / f'alphabet_{style}_{surface}'
            destination.mkdir(parents=True, exist_ok=True)
            for name, role in ROLES.items():
                for side in ('Sente', 'Gote'):
                    filename = f'{side}_{name}45.svg'
                    root = ET.parse(templates[f'pieces/{filename}']).getroot()
                    for parent in root.iter():
                        for child in list(parent):
                            if child.tag in (TAG('defs'), TAG('path')):
                                parent.remove(child)
                    polygon = next(root.iter(TAG('polygon')))
                    polygon.set('fill', face)
                    polygon.set('stroke', border)
                    polygon.set('class', 'piece-face')
                    parent = next(node for node in root.iter() if polygon in list(node))
                    ET.SubElement(parent, TAG('path'), {
                        'class': 'letter', 'fill': '#b3262e' if name in PROMOTED else '#111111',
                        'd': glyphs[role]})
                    title = root.find(TAG('title'))
                    title.text = f'{"後手" if side == "Gote" else "先手"} {role}{"（成）" if name in PROMOTED else ""}'
                    ET.indent(root, space='  ')
                    (destination / filename).write_text(ET.tostring(root, encoding='unicode') + '\n', encoding='utf-8')
    print('アルファベット駒 9セット × 30枚のSVGを生成しました。')


if __name__ == '__main__':
    main()
