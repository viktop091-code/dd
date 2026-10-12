#!/usr/bin/env python3
"""Create a deterministic, uncompressed Bohemia PBO archive."""

import argparse
import hashlib
import io
import struct
from pathlib import Path


PBO_PROPERTIES = 0x56657273


def write_cstring(stream, value):
    stream.write(value.encode("utf-8"))
    stream.write(b"\0")


def collect_files(source):
    files = []
    for path in sorted(source.rglob("*")):
        if path.is_file() and not any(part.startswith(".") for part in path.relative_to(source).parts):
            relative = path.relative_to(source).as_posix().replace("/", "\\")
            files.append((relative, path.read_bytes()))
    return files


def build_pbo(source, output, prefix):
    files = collect_files(source)
    if not files:
        raise ValueError(f"no files found under {source}")

    archive = io.BytesIO()

    write_cstring(archive, "")
    archive.write(struct.pack("<5I", PBO_PROPERTIES, 0, 0, 0, 0))
    write_cstring(archive, "product")
    write_cstring(archive, "dayz ugc")
    write_cstring(archive, "prefix")
    write_cstring(archive, prefix)
    write_cstring(archive, "")

    for relative, data in files:
        write_cstring(archive, relative)
        archive.write(struct.pack("<5I", 0, 0, 0, 0, len(data)))

    write_cstring(archive, "")
    archive.write(struct.pack("<5I", 0, 0, 0, 0, 0))

    for _, data in files:
        archive.write(data)

    payload = archive.getvalue()
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(payload + b"\0" + hashlib.sha1(payload).digest())
    return files


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--prefix", required=True)
    args = parser.parse_args()

    files = build_pbo(args.source.resolve(), args.output.resolve(), args.prefix)
    print(f"Packed {len(files)} files into {args.output}")


if __name__ == "__main__":
    main()
