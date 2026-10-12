#!/usr/bin/env python3
"""Apply the Wild-Fields package paths and visible branding to the imported base."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1] / "WildFields"
TEXT_SUFFIXES = {".c", ".cpp", ".xml", ".layout", ".json", ".csv"}

REPLACEMENTS = (
    ("AIConvoy/BundledConfigs/", "WildFields/BundledConfigs/"),
    ("AIConvoy/scripts", "WildFields/scripts"),
    ("$profile:AIConvoy", "$profile:WildFields"),
    ("AIConvoySettings.json", "WildFieldsSettings.json"),
    ("TriumphConvoyLoadout", "WildFieldsConvoyLoadout"),
    ("[AIConvoy]", "[Wild-Fields]"),
    ("Triumph Moving AI Convoy", "Wild-Fields"),
    ("AI Convoy", "Wild-Fields"),
)


def main():
    changed = 0
    for path in ROOT.rglob("*"):
        if not path.is_file() or path.suffix.lower() not in TEXT_SUFFIXES:
            continue
        text = path.read_text(encoding="utf-8-sig")
        updated = text
        for old, new in REPLACEMENTS:
            updated = updated.replace(old, new)
        if updated != text:
            path.write_text(updated, encoding="utf-8", newline="\n")
            changed += 1
    print(f"Rebranded {changed} imported files")


if __name__ == "__main__":
    main()
