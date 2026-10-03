#!/usr/bin/env python3
"""木目の駒から、現標準の糸柾を含む20種類の木肌・色調を再生成する。"""
from pathlib import Path
import argparse
import hashlib
import json
import random
import re
import xml.etree.ElementTree as ET

from PySide6.QtCore import Qt
from PySide6.QtGui import QPainterPath, QPainterPathStroker, QTransform

NS = 'http://www.w3.org/2000/svg'
ET.register_namespace('', NS)


def tag(name):
    return f'{{{NS}}}{name}'


def path(parent, d, color, opacity, width=None):
    a = {'d': d}
    if width is None:
        a.update(fill=color, opacity=f'{opacity:.4f}')
    else:
        a.update(fill='none', stroke=color, **{'stroke-opacity': f'{opacity:.4f}', 'stroke-width': f'{width:.3f}'})
    return ET.SubElement(parent, tag('path'), a)


def grain_path(data):
    """この生成器が使う絶対座標の M/L/C/S/Z を QPainterPath に変換する。"""
    tokens = iter(re.findall(r'[MLCSZ]|[-+]?(?:\d*\.\d+|\d+)', data))
    result = QPainterPath()
    previous_control = None
    for command in tokens:
        if command in ('M', 'L'):
            x, y = float(next(tokens)), float(next(tokens))
            if command == 'M':
                result.moveTo(x, y)
            else:
                result.lineTo(x, y)
            previous_control = None
        elif command in ('C', 'S'):
            if command == 'C':
                x1, y1 = float(next(tokens)), float(next(tokens))
            else:
                current = result.currentPosition()
                x1, y1 = current.x(), current.y()
                if previous_control is not None:
                    x1 = 2 * x1 - previous_control[0]
                    y1 = 2 * y1 - previous_control[1]
            x2, y2 = float(next(tokens)), float(next(tokens))
            x, y = float(next(tokens)), float(next(tokens))
            result.cubicTo(x1, y1, x2, y2, x, y)
            previous_control = x2, y2
        elif command == 'Z':
            result.closeSubpath()
            previous_control = None
        else:
            raise ValueError(f'未対応の木目パス: {command}')
    return result


def simplify_points(points, tolerance=0.005):
    """交差演算の細かすぎる折れ線を、見た目を保ったまま減らす。"""
    if len(points) <= 2:
        return points
    x, y = points[0]
    dx, dy = points[-1][0] - x, points[-1][1] - y
    length_squared = dx * dx + dy * dy
    distances = []
    for px, py in points[1:-1]:
        t = max(0, min(1, ((px - x) * dx + (py - y) * dy) / length_squared)) if length_squared else 0
        distances.append((px - x - t * dx) ** 2 + (py - y - t * dy) ** 2)
    farthest = max(distances)
    if farthest <= tolerance * tolerance:
        return [points[0], points[-1]]
    split = distances.index(farthest) + 1
    return simplify_points(points[:split + 1], tolerance)[:-1] + simplify_points(points[split:], tolerance)


def clip_grain(grain, polygon):
    """Qt SVG でも輪郭からはみ出さないよう、木目を生成時に切り抜く。"""
    outline = QPainterPath()
    for i, point in enumerate(polygon.get('points').split()):
        x, y = map(float, point.split(','))
        if i == 0:
            outline.moveTo(x, y)
        else:
            outline.lineTo(x, y)
    outline.closeSubpath()
    # 交差演算による曲線の直線近似を十分細かくする。外形・字形には触れない。
    scale = QTransform.fromScale(16, 16)
    outline = scale.map(outline)
    for element in list(grain):
        shape = grain_path(element.get('d'))
        color = element.get('fill')
        opacity = element.get('opacity')
        if color == 'none':
            stroker = QPainterPathStroker()
            stroker.setWidth(float(element.get('stroke-width')))
            stroker.setCapStyle(Qt.PenCapStyle.FlatCap)
            stroker.setJoinStyle(Qt.PenJoinStyle.MiterJoin)
            stroker.setCurveThreshold(0.01)
            shape = stroker.createStroke(shape)
            color = element.get('stroke')
            opacity = element.get('stroke-opacity')
        clipped = scale.map(shape).intersected(outline)
        if clipped.isEmpty():
            grain.remove(element)
            continue
        commands = []
        for subpath in clipped.toSubpathPolygons():
            points = simplify_points([(p.x() / 16, p.y() / 16) for p in subpath])
            commands.append('M' + ' L'.join(f'{x:.4f},{y:.4f}' for x, y in points) + ' Z')
        element.attrib.clear()
        element.attrib.update(d=' '.join(commands), fill=color, opacity=opacity)


def make_piece(source, design, destination):
    svg = ET.fromstring(source.read_text())
    defs = svg.find(tag('defs'))
    stops = defs.findall('.//' + tag('stop'))
    stops[0].set('stop-color', design['top'])
    stops[1].set('stop-color', design['bottom'])
    polygon = svg.find('.//' + tag('polygon'))
    polygon.set('stroke', design['edge'])
    glyph = next(p for p in svg.iter(tag('path')) if p.get('fill-rule'))
    promoted = glyph.get('fill') == '#aa1e21'
    glyph.set('fill', design['promoted'] if promoted else design['ink'])
    parent = next(p for p in svg.iter() if polygon in list(p))
    for p in list(parent):
        if p.tag == tag('path') and not p.get('fill-rule'):
            parent.remove(p)
    grain = ET.Element(tag('g'))
    parent.insert(list(parent).index(polygon) + 1, grain)
    seed = int(hashlib.sha256(source.name.encode()).hexdigest()[:8], 16)
    rng = random.Random(seed)
    strength = design['density']
    # Fine longitudinal fibers provide a common wooden base for all studies.
    for i in range(26):
        x = -1 + i * 1.85 + rng.uniform(-.45, .45)
        drift = rng.uniform(.2, 1.1)
        path(grain, f'M{x:.2f},0 C{x-drift:.2f},14 {x+drift:.2f},31 {x:.2f},47',
             design['grain'], strength * rng.uniform(.35, .75), rng.uniform(.12, .25))
    pattern = design['pattern']
    if pattern in ['tiger', 'silk', 'flow']:
        count = 6 if pattern == 'tiger' else 9 if pattern == 'silk' else 5
        for i in range(count):
            y = 2 + i * (42 / count) + rng.uniform(-1, 1)
            amplitude = rng.uniform(2.5, 5) if pattern != 'silk' else rng.uniform(1.1, 2.3)
            depth = rng.uniform(.65, 2.1) if pattern != 'silk' else rng.uniform(.35, .9)
            offset = rng.uniform(-3, 3)
            d = (f'M-3,{y:.2f} C8,{y-amplitude:.2f} 15,{y+amplitude:.2f} 25,{y+offset:.2f} '
                 f'S39,{y-amplitude:.2f} 48,{y+1:.2f} L48,{y+depth+1:.2f} '
                 f'C37,{y-amplitude+depth:.2f} 34,{y+offset+depth:.2f} 25,{y+offset+depth:.2f} '
                 f'S8,{y-amplitude+depth:.2f} -3,{y+depth:.2f} Z')
            path(grain, d, design['grain'], strength * rng.uniform(.8, 1.25))
            path(grain, f'M-3,{y-.4:.2f} C8,{y-amplitude-.4:.2f} 15,{y+amplitude-.4:.2f} 25,{y+offset-.4:.2f} S39,{y-amplitude-.4:.2f} 48,{y+.6:.2f}',
                 '#fff3d4', strength * .8, .45)
    elif pattern == 'curl':
        for i in range(9):
            x, y = 4 + i * 5, rng.uniform(9, 26)
            path(grain, f'M{x-8},-2 C{x+14},{y} {x-14},{y+8} {x+1},47', design['grain'], strength, .35)
    clip_grain(grain, polygon)
    ET.indent(svg, space='  ')
    destination.write_text('<?xml version="1.0" encoding="UTF-8"?>\n' + ET.tostring(svg, encoding='unicode') + '\n')


def main():
    repo = Path(__file__).resolve().parents[1]
    pieces = repo / 'resources/images/pieces'
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True, help='生成先ディレクトリ')
    args = parser.parse_args()
    designs = json.loads((pieces / 'variants.json').read_text())
    sources = sorted((pieces / 'wood_classic').glob('*.svg'))
    if len(sources) != 30:
        raise SystemExit('木目の駒が30枚必要です')
    for design in designs:
        target = args.output / design['style']
        target.mkdir(parents=True, exist_ok=True)
        for source in sources:
            make_piece(source, design, target / source.name)
    print(f'{len(designs)}種類、{len(designs) * len(sources)}枚を生成しました: {args.output}')


if __name__ == '__main__':
    main()
