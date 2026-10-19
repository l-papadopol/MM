#!/usr/bin/env python3
"""Preserve release notes while stamping the distribution's machine identity."""
import argparse
from pathlib import Path
import re

def stamp(notes: str, build: int, commit: str) -> str:
    if not 0 < build <= 2147483647 or not re.fullmatch(r'(?:[0-9a-fA-F]{40}|[0-9a-fA-F]{64})', commit):
        raise ValueError('Invalid distribution identity')
    notes = re.sub(r'^<!-- MadModem (?:build|commit): [^\r\n]* -->[ \t]*\r?\n?', '', notes, flags=re.M)
    return notes.rstrip() + f'\n\n<!-- MadModem build: {build} -->\n<!-- MadModem commit: {commit.lower()} -->\n'

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=Path);p.add_argument('output', type=Path)
    p.add_argument('build', type=int);p.add_argument('commit')
    a=p.parse_args();a.output.write_text(stamp(a.source.read_text(), a.build, a.commit))
if __name__=='__main__': main()
