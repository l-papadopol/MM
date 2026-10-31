#!/usr/bin/env python3
"""Remove only the explicitly retired project documents after a source overlay."""
import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    flags = parser.add_mutually_exclusive_group()
    flags.add_argument('--apply', action='store_true')
    flags.add_argument('--check', action='store_true')
    args = parser.parse_args()
    paths = []
    for name in json.loads((ROOT / 'packaging/retired-docs.json').read_text()):
        path = ROOT / name
        if not path.resolve().is_relative_to(ROOT):
            raise SystemExit('Refusing a retired path outside the source directory')
        if path.is_file():
            paths.append(path)
    if args.apply:
        for path in paths:
            path.unlink()
        print(f'Removed {len(paths)} retired documents. Commit tracked deletions with the new sources.')
    elif paths:
        print(f'{len(paths)} retired documents remain. Run: python3 scripts/clean_legacy_docs.py --apply')
        if args.check:
            return 1
    else:
        print('Current documentation tree: no retired files remain.')
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
