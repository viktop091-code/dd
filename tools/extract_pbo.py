#!/usr/bin/env python3
"""Extract uncompressed files from a Bohemia PBO archive."""

import argparse
import struct
from pathlib import Path


PBO_PROPERTIES = 0x56657273


def read_cstring(stream):
    value = bytearray()
    while True:
        char = stream.read(1)
        if not char:
            raise EOFError("unexpected end of PBO")
        if char == b"\0":
            return value.decode("utf-8", errors="replace")
        value.extend(char)


def extract_pbo(archive_path, output_dir):
    entries = []
    with archive_path.open("rb") as stream:
        name = read_cstring(stream)
        method, original_size, reserved, timestamp, stored_size = struct.unpack("<5I", stream.read(20))

        if not name and method == PBO_PROPERTIES:
            while read_cstring(stream):
                read_cstring(stream)
            name = read_cstring(stream)
            method, original_size, reserved, timestamp, stored_size = struct.unpack("<5I", stream.read(20))

        while name:
            if method != 0:
                raise ValueError(f"compressed PBO entry is unsupported: {name}")
            entries.append((name, stored_size))
            name = read_cstring(stream)
            method, original_size, reserved, timestamp, stored_size = struct.unpack("<5I", stream.read(20))

        for name, stored_size in entries:
            relative = Path(*name.replace("\\", "/").split("/"))
            destination = output_dir / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            destination.write_bytes(stream.read(stored_size))

    return entries


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("archive", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    entries = extract_pbo(args.archive.resolve(), args.output.resolve())
    print(f"Extracted {len(entries)} files into {args.output}")


if __name__ == "__main__":
    main()
