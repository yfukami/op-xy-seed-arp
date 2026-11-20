#!/usr/bin/env python3
"""Gives the starter its real name, in place: python3 tools/rename.py <id> "<Display Name>"

The ID is what the OP-XY's player menu shows, and it is permanent once in the
catalog: a lowercase letter, then letters, digits or _, at most 12 characters
(the menu cut a 13-character ID). It replaces `newplayer` everywhere, renames the
machine folder, sets the display name and regenerates the artwork.
"""
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OLD = 'newplayer'
TEXT = ('.md', '.json', '.cpp', '.hpp', '.py', '.fragment')


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    new, name = sys.argv[1], sys.argv[2]
    if not re.fullmatch(r'[a-z][a-z0-9_]{0,11}', new):
        sys.exit('The ID is a lowercase letter, then letters, digits or _, at most 12 characters.')
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9 _-]{0,63}', name):
        sys.exit('The display name is plain letters, digits, spaces, _ or -.')
    folder = ROOT / 'machines' / OLD
    if not folder.is_dir():
        sys.exit(f'Already renamed: machines/{OLD} is gone.')
    for path in sorted(ROOT.rglob('*')):
        if '.git' in path.parts or not path.is_file() or path.suffix not in TEXT or path.name == 'rename.py':
            continue
        text = path.read_text(encoding='utf-8')
        if OLD in text:
            path.write_text(text.replace(OLD, new), encoding='utf-8')
    folder.rename(ROOT / 'machines' / new)
    definition = ROOT / 'machines' / new / 'definition.json'
    value = json.loads(definition.read_text(encoding='utf-8'))
    value['name'] = name
    definition.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')
    subprocess.run([sys.executable, str(ROOT / 'tools' / 'gen_panel.py')], check=True)
    print(f'Renamed to {new} ("{name}"). Run the CLAUDE.md test command, then commit.')


if __name__ == '__main__':
    main()
