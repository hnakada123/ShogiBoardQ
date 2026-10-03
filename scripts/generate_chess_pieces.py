#!/usr/bin/env python3
"""HTMLから書き出した絵柄を、標準駒の外形・縮尺・先後の向きに組み込む。

node scripts/export_chess_symbols.mjs /tmp/shogi-chess-symbols
python3 scripts/generate_chess_pieces.py /tmp/shogi-chess-symbols
"""
import argparse
import base64
from pathlib import Path
import xml.etree.ElementTree as ET

NS = 'http://www.w3.org/2000/svg'
XLINK = 'http://www.w3.org/1999/xlink'
ET.register_namespace('', NS)
ET.register_namespace('xlink', XLINK)
ROLES = {'ou': 'K', 'gyoku': 'K', 'hi': 'R', 'kaku': 'B', 'kin': 'G',
         'gin': 'S', 'kei': 'N', 'kyou': 'L', 'fu': 'P', 'ryuu': 'R',
         'uma': 'B', 'narigin': 'S', 'narikei': 'N', 'narikyou': 'L', 'to': 'P'}
PROMOTED = {'ryuu', 'uma', 'narigin', 'narikei', 'narikyou', 'to'}
SURFACES = {'wood': ('#fcf0d5', '#9b8664'), 'paper': ('#fffef8', '#afb3a5'),
            'slate': ('#f6ecda', '#b4a590')}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('symbols', type=Path)
    args = parser.parse_args()
    pieces = Path(__file__).resolve().parents[1] / 'resources/images/pieces'
    for design in ('facet', 'atelier', 'ribbon'):
        for surface, (fill, stroke) in SURFACES.items():
            destination = pieces / f'chess_{design}_{surface}'
            destination.mkdir(exist_ok=True)
            for name, role in ROLES.items():
                symbol = args.symbols / f'{design}-{role}{"-promoted" if name in PROMOTED else ""}.png'
                data = base64.b64encode(symbol.read_bytes()).decode('ascii')
                for side in ('Sente', 'Gote'):
                    filename = f'{side}_{name}45.svg'
                    root = ET.parse(pieces / 'wood_classic' / filename).getroot()
                    for parent in root.iter():
                        for child in list(parent):
                            if child.tag in (f'{{{NS}}}defs', f'{{{NS}}}path'):
                                parent.remove(child)
                    polygon = next(root.iter(f'{{{NS}}}polygon'))
                    polygon.set('fill', fill)
                    polygon.set('stroke', stroke)
                    parent = next(node for node in root.iter() if polygon in list(node))
                    ET.SubElement(parent, f'{{{NS}}}image', {
                        'x': '0', 'y': '0', 'width': '45', 'height': '45',
                        f'{{{XLINK}}}href': 'data:image/png;base64,' + data})
                    ET.indent(root, space='  ')
                    (destination / filename).write_text('<?xml version="1.0" encoding="UTF-8"?>\n'
                        + ET.tostring(root, encoding='unicode') + '\n', encoding='utf-8')
            print(f'{destination.name}: 30 SVGs')


if __name__ == '__main__':
    main()
