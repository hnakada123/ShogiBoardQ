#!/usr/bin/env python3
"""生成サンプルの字形をベクター化し、標準と同じ外形の戦国文字駒を作る。

再生成には Pillow と potrace が必要。アプリ実行時には不要。
サンプルPNGは変更せず、抽出した字形のみをSVGのパスとして保存する。
"""
import argparse
import copy
from pathlib import Path
import subprocess
import tempfile
import xml.etree.ElementTree as ET

from PIL import Image, ImageDraw

NS = 'http://www.w3.org/2000/svg'
ET.register_namespace('', NS)

# 採用した一覧画像の字形領域。枠を含めず、元画像の座標で指定する。
ROWS = (
    (('ou', 'gyoku', 'hi', 'kaku', 'kin'), 68, 279),
    (('gin', 'kei', 'kyou', 'fu', 'to'), 385, 600),
    (('narigin', 'narikei', 'narikyou', 'uma', 'ryuu'), 690, 897),
)
CENTERS = (172, 490, 809, 1128, 1447)
PROMOTED = {'to', 'narigin', 'narikei', 'narikyou', 'uma', 'ryuu'}


def tag(name):
    return f'{{{NS}}}{name}'


def trace_glyph(image, box, work):
    """駒面の濃い字形を抽出し、穴を保ったベクターパスに変換する。"""
    region = image.crop(box)
    mask = Image.new('1', region.size, 1)
    source, target = region.load(), mask.load()
    for y in range(region.height):
        for x in range(region.width):
            red, green, blue, alpha = source[x, y]
            if alpha >= 200 and red + green + blue < 450:
                target[x, y] = 0
    # 領域の上隅に入るサンプルの斜めの外枠を、端に接する成分ごと除く。
    edges = ([(x, y) for x in range(mask.width) for y in (0, mask.height - 1)]
             + [(x, y) for y in range(mask.height) for x in (0, mask.width - 1)])
    for point in edges:
        if mask.getpixel(point) == 0:
            ImageDraw.floodfill(mask, point, 1)
    bounds = mask.convert('L').point(lambda value: 255 - value).getbbox()
    if bounds is None:
        raise ValueError(f'字形がありません: {box}')
    mask = mask.crop(bounds)
    bitmap, vector = work / 'glyph.pbm', work / 'glyph.svg'
    mask.save(bitmap)
    subprocess.run(['potrace', '--svg', '--turdsize', '3', '--opttolerance', '0.2',
                    '--output', str(vector), str(bitmap)], check=True)
    traced = ET.parse(vector).getroot().find(tag('g'))
    if traced is None or not list(traced):
        raise ValueError(f'ベクター化に失敗しました: {box}')
    # 文字幅と高さを揃えつつ縦横比を維持。全駒共通の輪郭内に収める。
    scale = min(26.0 / mask.width, 27.0 / mask.height)
    x = 22.5 - mask.width * scale / 2
    y = 24.0 - mask.height * scale / 2
    group = ET.Element(tag('g'), transform=f'translate({x:.5f},{y:.5f}) scale({scale:.7f})')
    traced.attrib.pop('fill', None)
    group.append(traced)
    return group


def make_piece(template, glyph, promoted, destination):
    svg = ET.parse(template).getroot()
    # polygonと、その祖先にある種類別縮尺・先後の回転をそのまま使用する。
    for parent in svg.iter():
        for element in list(parent):
            if element.tag in (tag('defs'), tag('path')):
                parent.remove(element)
    polygon = next(svg.iter(tag('polygon')))
    polygon.set('fill', '#f5e8ce')
    polygon.set('stroke', '#735d41')
    parent = next(node for node in svg.iter() if polygon in list(node))
    symbol = copy.deepcopy(glyph)
    symbol.set('fill', '#b74032' if promoted else '#292721')
    parent.append(symbol)
    ET.indent(svg, space='  ')
    destination.write_text('<?xml version="1.0" encoding="UTF-8"?>\n'
                           + ET.tostring(svg, encoding='unicode') + '\n', encoding='utf-8')


def main():
    repo = Path(__file__).resolve().parents[1]
    pieces = repo / 'resources/images/pieces'
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True, help='採用サンプルのsengoku_kanji.png')
    parser.add_argument('--output', type=Path, required=True, help='SVG30枚の生成先')
    args = parser.parse_args()
    with Image.open(args.source.expanduser()) as source:
        image = source.convert('RGBA')
    if image.size != (1619, 971):
        raise ValueError('採用サンプルの寸法が変わっています。字形領域を見直してください。')
    args.output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='shogiboardq-sengoku-') as temporary:
        for names, top, bottom in ROWS:
            for name, center in zip(names, CENTERS):
                glyph = trace_glyph(image, (center - 108, top, center + 108, bottom), Path(temporary))
                for side in ('Sente', 'Gote'):
                    filename = f'{side}_{name}45.svg'
                    make_piece(pieces / 'standard' / filename, glyph, name in PROMOTED,
                               args.output / filename)
    print(f'{args.output}: 戦国文字のSVG30枚を生成しました。')


if __name__ == '__main__':
    main()
