#!/usr/bin/env python3
"""Generates the machine's panel.svg.fragment, or a standalone review SVG.

  python3 tools/gen_panel.py
  python3 tools/gen_panel.py --state 0x2405,11,5 --text "C4  #0.42" --out /path/to/review.svg

The page shows a strip of 16 step cells, the half of the pattern holding the
playhead: dots 0..15 light the steps that play, and the playhead flips its
cell. Dots 16..23 mark the loop's end: dots 16..19 each draw a bracket after
every fourth cell, and dots 20..23 are black covers over each group's gaps,
all lit but the end's group. --state is cells,end,playhead: a 16-bit mask of
the steps that play, the loop's last cell (-1 if not in this half) and the
playhead cell (-1 when stopped), as display() in player.cpp shows them.
Everything is drawn with straight-line <path> elements: the OP-XY did not draw
<circle>. Artwork stays below the header (y = 38).
"""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FOLDER = next(p.parent for p in (ROOT / 'machines').glob('*/definition.json'))
PREFIX = json.loads((FOLDER / 'definition.json').read_text(encoding='utf-8'))['id']
OUTPUT = FOLDER / 'panel.svg.fragment'
X0, DX, CELL_W = 52, 24, 16     # 16 cells from x = 52 to 428
CELL_TOP, CELL_H = 118, 34
BASE_Y, BASE_H = 156, 3
END_TOP, END_H = 112, 52     # the loop-end bracket spans the cells and their baselines


def n(value):
    text = f'{value:.3f}'.rstrip('0').rstrip('.')
    return '0' if text in ('-0', '') else text


def outline(points):
    return 'M' + ' L'.join(f'{n(x)} {n(y)}' for x, y in points) + ' Z'


def rect(x, y, w, h):
    return f'M{n(x)} {n(y)} L{n(x + w)} {n(y)} L{n(x + w)} {n(y + h)} L{n(x)} {n(y + h)} Z'


def at(x, y):
    return f'transform="matrix(1 0 0 1 {n(x)} {n(y)})"'


def paint(colour):
    return f'fill="{colour}" stroke="{colour}" stroke-width="1"'


def cell_x(i):
    return X0 + DX * i


def bracket(x):
    """A "]" in the gap after the cell at x (relative to the first cell)."""
    left = x + CELL_W + 2
    return outline([(left, 0), (left + 5, 0), (left + 5, END_H), (left, END_H), (left, END_H - 2), (left + 3, END_H - 2),
                    (left + 3, 2), (left, 2)])


def dots():
    """The 24 bound dots in dot order. Paint order puts brackets before their covers."""
    result = [(i, f'<path id="{PREFIX}_dot_{i}" {at(cell_x(i), CELL_TOP)} d="{rect(0, 0, CELL_W, CELL_H)}" '
                  f'{paint("#FFFFFF")}/>') for i in range(16)]
    for r in range(4):
        # A loop-end bracket in the gap after cells r, r+4, r+8 and r+12.
        d = ' '.join(bracket(cell_x(r + 4 * g) - X0) for g in range(4))
        result.append((16 + r, f'<path id="{PREFIX}_dot_{16 + r}" {at(X0, END_TOP)} d="{d}" {paint("#FFFFFF")}/>'))
    for g in range(4):
        # Black covers over the gaps after group g's four cells only, never the cells.
        d = ' '.join(rect(cell_x(4 * g + i) - X0 + CELL_W + 1, -1, DX - CELL_W - 2, END_H + 2) for i in range(4))
        result.append((20 + g, f'<path id="{PREFIX}_dot_{20 + g}" {at(X0, END_TOP)} d="{d}" {paint("#000000")}/>'))
    return result


def static_art():
    """A baseline under every cell, brighter on the beats (steps 1, 5, 9 and 13)."""
    return [f'<path {at(cell_x(i), BASE_Y)} d="{rect(0, 0, CELL_W, BASE_H)}" '
            f'{paint("#BBBBBB" if i % 4 == 0 else "#666666")}/>' for i in range(16)]


def text_node(content):
    return (f'<text id="{PREFIX}_text" transform="matrix(1 0 0 1 240 92)" fill="#FFFFFF" '
            f'text-anchor="middle" font-family="UniversNextPro-Light" font-size="24">{content}</text>')


def fragment():
    body = static_art() + [svg for _, svg in dots()] + [text_node('seed 0')]
    return f'<g id="{PREFIX}_panel">\n' + '\n'.join(body) + '\n</g>\n'


def lit_mask(cells, end, playhead):
    mask = cells & 0xffff
    if 0 <= playhead < 16:
        mask ^= 1 << playhead
    if 0 <= end < 16:
        mask |= 1 << (16 + end % 4)
        for g in range(4):
            if g != end // 4:
                mask |= 1 << (20 + g)
    return mask


def review(cells, end, playhead, content):
    mask = lit_mask(cells, end, playhead)
    body = '\n'.join(static_art() + [svg for i, svg in dots() if mask >> i & 1] + [text_node(content)])
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="960" height="444" viewBox="0 0 480 222">\n'
            '<rect width="480" height="222" fill="#000000"/>\n<rect width="480" height="38" fill="#1C1C1C"/>\n'
            + body + '\n</svg>\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--state', help='cells,end,playhead: lit cells (16-bit mask), the loop end cell and the playhead cell (-1 for none)')
    parser.add_argument('--text', default='C4  #0.42')
    parser.add_argument('--out', type=Path)
    args = parser.parse_args()
    if args.state:
        if not args.out:
            parser.error('--state needs --out')
        cells, end, playhead = (int(part, 0) for part in args.state.split(','))
        args.out.write_text(review(cells, end, playhead, args.text), encoding='utf-8')
    else:
        (args.out or OUTPUT).write_text(fragment(), encoding='utf-8')


if __name__ == '__main__':
    main()
