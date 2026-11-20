#!/usr/bin/env python3
"""Generates the machine's panel.svg.fragment, or a standalone review SVG.

  python3 tools/gen_panel.py
  python3 tools/gen_panel.py --lit 3 --text playing --out /path/to/review.svg

A player page has 24 dots that show or hide and one centred text line. Draw
everything with straight-line <path> elements: the OP-XY did not draw <circle>.
Keep artwork below y=38 (the header) and within 480 x 222.
"""
import argparse
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FOLDER = next(p.parent for p in (ROOT / 'machines').glob('*/definition.json'))
PREFIX = json.loads((FOLDER / 'definition.json').read_text(encoding='utf-8'))['id']
DOTS, DOT_R, Y = 24, 5, 110


def n(value):
    text = f'{value:.3f}'.rstrip('0').rstrip('.')
    return '0' if text in ('-0', '') else text


def disc(radius, sides=16):
    points = [(radius * math.sin(2 * math.pi * i / sides), -radius * math.cos(2 * math.pi * i / sides))
              for i in range(sides)]
    return 'M' + ' L'.join(f'{n(x)} {n(y)}' for x, y in points) + ' Z'


def dot(i):
    x = 240 + (i - (DOTS - 1) / 2) * 17
    return (f'<path id="{PREFIX}_dot_{i}" transform="matrix(1 0 0 1 {n(x)} {Y})" d="{disc(DOT_R)}" '
            'fill="#FFFFFF" stroke="#FFFFFF" stroke-width="1"/>')


def text(content):
    return (f'<text id="{PREFIX}_text" transform="matrix(1 0 0 1 240 170)" fill="#FFFFFF" '
            f'text-anchor="middle" font-family="UniversNextPro-Light" font-size="26">{content}</text>')


def fragment():
    return f'<g id="{PREFIX}_panel">\n' + '\n'.join([dot(i) for i in range(DOTS)] + [text('ready')]) + '\n</g>\n'


def review(lit, content):
    body = '\n'.join([dot(i) for i in range(min(lit, DOTS))] + [text(content)])
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="960" height="444" viewBox="0 0 480 222">\n'
            '<rect width="480" height="222" fill="#000000"/>\n<rect width="480" height="38" fill="#1C1C1C"/>\n'
            + body + '\n</svg>\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--lit', type=int)
    parser.add_argument('--text', default='playing')
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    if args.lit is not None:
        if not args.out:
            parser.error('--lit needs --out')
        args.out.write_text(review(args.lit, args.text), encoding='utf-8')
    else:
        (args.out or FOLDER / 'panel.svg.fragment').write_text(fragment(), encoding='utf-8')


if __name__ == '__main__':
    main()
