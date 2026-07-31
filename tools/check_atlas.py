#!/usr/bin/env python3
"""Verifies the sprite atlas matches the SpriteId enum.

assets/atlas.json is generated art and src/Core/SpriteId.h is hand-written C++,
but the sprite names are a hard contract between them. A mismatch does not fail
the build -- the renderer logs a warning and silently substitutes a white quad,
which is easy to miss and confusing to debug. Checking it takes milliseconds.

Exits non-zero on any mismatch so CI fails loudly.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SPRITE_ID_HEADER = REPO_ROOT / "src" / "Core" / "SpriteId.h"
ATLAS_JSON = REPO_ROOT / "assets" / "atlas.json"

# Matches the `case SpriteId::Foo: return "foo";` lines in spriteName().
_NAME_PATTERN = re.compile(r'return\s+"([a-z0-9_]+)"\s*;')


def enum_sprite_names(header: Path) -> set[str]:
    if not header.is_file():
        sys.exit(f"ERROR: {header} not found")
    return set(_NAME_PATTERN.findall(header.read_text(encoding="utf-8")))


def atlas_sprite_names(atlas: Path) -> set[str]:
    if not atlas.is_file():
        sys.exit(
            f"ERROR: {atlas} not found.\n"
            f"Regenerate it with: python tools/generate_art.py --out assets"
        )
    try:
        data = json.loads(atlas.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        sys.exit(f"ERROR: {atlas} is not valid JSON: {exc}")

    sprites = data.get("sprites")
    if not isinstance(sprites, dict):
        sys.exit(f"ERROR: {atlas} has no 'sprites' object")
    return set(sprites)


def main() -> int:
    enum_names = enum_sprite_names(SPRITE_ID_HEADER)
    atlas_names = atlas_sprite_names(ATLAS_JSON)

    missing = sorted(enum_names - atlas_names)   # enum expects, atlas lacks
    extra = sorted(atlas_names - enum_names)     # atlas has, nothing uses

    print(f"SpriteId enum : {len(enum_names)} names")
    print(f"atlas.json    : {len(atlas_names)} sprites")

    if missing:
        print("\nERROR: sprites referenced by SpriteId.h but missing from the atlas.")
        print("These would render as blank white quads at runtime:")
        for name in missing:
            print(f"  - {name}")

    if extra:
        # Not fatal: unused art wastes atlas space but breaks nothing.
        print("\nWARNING: sprites in the atlas that no SpriteId refers to:")
        for name in extra:
            print(f"  - {name}")

    if missing:
        return 1

    print("\nOK: every SpriteId resolves to a sprite in the atlas.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
