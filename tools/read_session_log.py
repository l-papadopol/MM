#!/usr/bin/env python3
"""Extract MadModem .jsonl.gz (or legacy .jsonl), with optional crash recovery."""
import argparse
import gzip
from pathlib import Path
import shutil
import sys
import zlib

CHUNK = 128 * 1024


def recover(source, target):
    """Emit complete lines only. A missing gzip trailer remains an error."""
    decoder = zlib.decompressobj(16 + zlib.MAX_WBITS)
    pending = b""
    seen = False
    try:
        while data := source.read(CHUNK):
            seen = True
            drain = False
            while data or drain:
                if decoder.eof:
                    decoder = zlib.decompressobj(16 + zlib.MAX_WBITS)
                plain = decoder.decompress(data, CHUNK)
                data = decoder.unused_data if decoder.eof else decoder.unconsumed_tail
                drain = len(plain) == CHUNK and not decoder.eof
                pending += plain
                end = pending.rfind(b"\n") + 1
                if end:
                    target.write(pending[:end])
                    pending = pending[end:]
    except zlib.error as error:
        print(f"Recovered prefix only: damaged gzip stream ({error}).", file=sys.stderr)
        return 2
    if not seen or not decoder.eof or pending:
        print("Recovered complete lines only: gzip stream is incomplete (live file or interrupted process).", file=sys.stderr)
        return 2
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("-o", "--output", type=Path, help="new JSONL file; existing files are never overwritten")
    parser.add_argument("--recover", action="store_true", help="recover complete lines from a live/truncated gzip; exit 2 indicates incomplete data")
    args = parser.parse_args()
    output = None
    try:
        output = args.output.open("xb") if args.output else sys.stdout.buffer
        if args.input.suffix.lower() == ".gz" and args.recover:
            with args.input.open("rb") as source:
                return recover(source, output)
        opener = gzip.open if args.input.suffix.lower() == ".gz" else open
        with opener(args.input, "rb") as source:
            shutil.copyfileobj(source, output, CHUNK)
        return 0
    except (OSError, EOFError, zlib.error) as error:
        print(f"Cannot extract log: {error}. Use --recover for an interrupted gzip stream.", file=sys.stderr)
        return 2
    finally:
        if args.output and output is not None:
            output.close()


if __name__ == "__main__":
    raise SystemExit(main())
