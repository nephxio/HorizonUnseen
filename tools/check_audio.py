#!/usr/bin/env python3
"""Verifies the sound manifest matches the SoundId and MusicId enums.

assets/sounds.json is generated audio and src/Core/SoundId.h is hand-written
C++, but the sound names are a hard contract between them. A mismatch does not
fail the build -- the audio engine logs a warning and plays nothing, which is
indistinguishable from a volume slider being down and miserable to debug.

Goes further than check_atlas.py in one respect: it also opens each referenced
wav. A manifest entry pointing at a missing or truncated file is exactly as
silent as a missing entry, and just as easy to introduce by moving files around.

Exits non-zero on any mismatch so CI fails loudly.
"""

from __future__ import annotations

import json
import re
import sys
import wave
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SOUND_ID_HEADER = REPO_ROOT / "src" / "Core" / "SoundId.h"
ASSET_DIR = REPO_ROOT / "assets"
SOUNDS_JSON = ASSET_DIR / "sounds.json"

# Matches the `case SoundId::Foo: return "foo";` lines. Music names are
# distinguished by their required prefix rather than by parsing two functions,
# which keeps this robust against the file being reordered.
_NAME_PATTERN = re.compile(r'return\s+"([a-z0-9_]+)"\s*;')
_MUSIC_PREFIX = "music_"


def enum_names(header: Path) -> tuple[set[str], set[str]]:
    if not header.is_file():
        sys.exit(f"ERROR: {header} not found")
    found = set(_NAME_PATTERN.findall(header.read_text(encoding="utf-8")))
    music = {n for n in found if n.startswith(_MUSIC_PREFIX)}
    return found - music, music


def manifest_names(path: Path) -> tuple[dict, set[str], set[str]]:
    if not path.is_file():
        sys.exit(
            f"ERROR: {path} not found.\n"
            f"Regenerate it with: python tools/generate_audio.py --out assets"
        )
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        sys.exit(f"ERROR: {path} is not valid JSON: {exc}")

    for section in ("sounds", "music"):
        if not isinstance(data.get(section), dict):
            sys.exit(f"ERROR: {path} has no '{section}' object")

    return data, set(data["sounds"]), set(data["music"])


def check_files(data: dict) -> list[str]:
    """Confirms every referenced wav exists and is a readable, non-empty
    16-bit file. Returns a list of problems."""
    problems: list[str] = []
    for section in ("sounds", "music"):
        for name, entry in sorted(data[section].items()):
            rel = entry.get("file")
            if not rel:
                problems.append(f"{name}: manifest entry has no 'file'")
                continue

            path = ASSET_DIR / rel
            if not path.is_file():
                problems.append(f"{name}: {rel} does not exist")
                continue

            try:
                with wave.open(str(path)) as w:
                    channels = w.getnchannels()
                    width = w.getsampwidth()
                    count = w.getnframes()
            except (wave.Error, EOFError) as exc:
                problems.append(f"{name}: {rel} is not a readable wav ({exc})")
                continue

            if count == 0:
                problems.append(f"{name}: {rel} contains no audio")
            if width != 2:
                problems.append(f"{name}: {rel} is {width * 8}-bit, expected 16")

            # Effects are panned by screen position, and OpenAL only spatialises
            # mono sources -- a stereo effect would silently ignore its pan.
            if section == "sounds" and channels != 1:
                problems.append(
                    f"{name}: {rel} is stereo; effects must be mono to be panned")
    return problems


def report(label: str, enum_set: set[str], manifest_set: set[str]) -> bool:
    missing = sorted(enum_set - manifest_set)
    extra = sorted(manifest_set - enum_set)

    print(f"{label:<14} enum {len(enum_set):>3}   manifest {len(manifest_set):>3}")

    if missing:
        print(f"\nERROR: {label} referenced by SoundId.h but missing from the manifest.")
        print("These would be silent at runtime:")
        for name in missing:
            print(f"  - {name}")

    if extra:
        # Not fatal: an unreferenced sound wastes disk but breaks nothing.
        print(f"\nWARNING: {label} in the manifest that no enum entry refers to:")
        for name in extra:
            print(f"  - {name}")

    return bool(missing)


def main() -> int:
    enum_sounds, enum_music = enum_names(SOUND_ID_HEADER)
    data, manifest_sounds, manifest_music = manifest_names(SOUNDS_JSON)

    failed = report("effects", enum_sounds, manifest_sounds)
    failed |= report("music", enum_music, manifest_music)

    problems = check_files(data)
    if problems:
        print("\nERROR: problems with the referenced audio files:")
        for problem in problems:
            print(f"  - {problem}")
        failed = True

    if failed:
        return 1

    total = len(manifest_sounds) + len(manifest_music)
    print(f"\nOK: every SoundId and MusicId resolves to a playable file ({total} checked).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
