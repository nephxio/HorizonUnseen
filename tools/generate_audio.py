#!/usr/bin/env python3
"""
generate_audio.py - synthesise all placeholder audio for Horizon Unseen.

Produces:
    <out>/sounds/*.wav   one-shot effects (mono) and music beds (stereo)
    <out>/sounds.json    { sample_rate, sounds: {...}, music: {...} }

The sound name lists are a hard contract with src/Core/SoundId.h. Every name in
REQUIRED_SOUNDS and REQUIRED_MUSIC below must exist in the JSON, and
tools/check_audio.py fails CI when the two disagree.

Effects are mono on purpose. OpenAL only spatialises mono sources -- a stereo
buffer bypasses panning entirely -- and every effect is positioned in the
stereo field by where it happened on screen. Music is stereo and never panned,
so it is generated at a lower sample rate to keep the committed files small.

Everything is built from oscillators and shaped noise with a seeded RNG, so
re-running reproduces byte-for-byte output and no binary source audio is
committed. Standard library only: no numpy, no Pillow.

Usage:
    python tools/generate_audio.py --out assets
    python tools/generate_audio.py --out assets --music-seconds 8
"""

from __future__ import annotations

import argparse
import json
import math
import os
import random
import struct
import sys
import wave

# ---------------------------------------------------------------------------
# Contract: these names must all end up as keys in sounds.json, and must match
# soundName()/musicName() in src/Core/SoundId.h.
# ---------------------------------------------------------------------------

REQUIRED_SOUNDS = [
    "weapon_fire",
    "enemy_fire",
    "impact",
    "explosion",
    "big_explosion",
    "debris",
    "powerup_pickup",
    "cell_charge",
    "cell_break",
    "superweapon_charge",
    "screen_clear",
    "graze",
    "secret_found",
    "ui_move",
    "ui_select",
    "ui_back",
]

REQUIRED_MUSIC = [
    "music_menu",
    "music_level",
    "music_boss",
]

SFX_RATE = 44100
MUSIC_RATE = 22050

# ---------------------------------------------------------------------------
# Signal helpers
#
# Everything is a plain list of floats in roughly [-1, 1]; clipping happens once
# at write time.
# ---------------------------------------------------------------------------


def frames(seconds: float, rate: int) -> int:
    return max(1, int(seconds * rate))


def silence(seconds: float, rate: int) -> list[float]:
    return [0.0] * frames(seconds, rate)


def sine(buf: list[float], freq_fn, amp_fn, rate: int, phase: float = 0.0) -> None:
    """Adds a sine whose frequency and amplitude are functions of t in [0, 1]."""
    n = len(buf)
    for i in range(n):
        t = i / n
        phase += 2.0 * math.pi * freq_fn(t) / rate
        buf[i] += math.sin(phase) * amp_fn(t)


def saw(buf: list[float], freq_fn, amp_fn, rate: int) -> None:
    n = len(buf)
    phase = 0.0
    for i in range(n):
        t = i / n
        phase += freq_fn(t) / rate
        phase -= math.floor(phase)
        buf[i] += (2.0 * phase - 1.0) * amp_fn(t)


def square(buf: list[float], freq_fn, amp_fn, rate: int, duty: float = 0.5) -> None:
    n = len(buf)
    phase = 0.0
    for i in range(n):
        t = i / n
        phase += freq_fn(t) / rate
        phase -= math.floor(phase)
        buf[i] += (1.0 if phase < duty else -1.0) * amp_fn(t)


def noise(buf: list[float], amp_fn, rng: random.Random) -> None:
    n = len(buf)
    for i in range(n):
        buf[i] += rng.uniform(-1.0, 1.0) * amp_fn(i / n)


def lowpass(buf: list[float], cutoff_fn, rate: int) -> None:
    """One-pole lowpass, cutoff in Hz as a function of t. Cheap but enough to
    turn white noise into something that reads as an explosion."""
    y = 0.0
    n = len(buf)
    for i in range(n):
        cutoff = max(20.0, min(cutoff_fn(i / n), rate * 0.45))
        alpha = 1.0 - math.exp(-2.0 * math.pi * cutoff / rate)
        y += alpha * (buf[i] - y)
        buf[i] = y


def highpass(buf: list[float], cutoff: float, rate: int) -> None:
    alpha = 1.0 - math.exp(-2.0 * math.pi * cutoff / rate)
    y = 0.0
    for i in range(len(buf)):
        y += alpha * (buf[i] - y)
        buf[i] = buf[i] - y


def decay(power: float = 3.0):
    """Exponential-ish fall from 1 to 0."""
    return lambda t: (1.0 - t) ** power


def attack_decay(attack: float, power: float = 3.0):
    def env(t: float) -> float:
        if t < attack:
            return t / attack if attack > 0 else 1.0
        u = (t - attack) / max(1e-6, 1.0 - attack)
        return (1.0 - u) ** power
    return env


def const(value: float):
    return lambda _t: value


def sweep(start: float, end: float, curve: float = 1.0):
    return lambda t: start + (end - start) * (t ** curve)


def declick(buf: list[float], rate: int, ms: float = 3.0) -> None:
    """Fades the first and last few milliseconds so a buffer cannot start or end
    on a discontinuity, which otherwise reads as a click on every playback."""
    n = len(buf)
    edge = min(frames(ms / 1000.0, rate), n // 2)
    for i in range(edge):
        g = i / edge
        buf[i] *= g
        buf[n - 1 - i] *= g


def normalise(buf: list[float], peak: float = 0.9) -> None:
    high = max((abs(v) for v in buf), default=0.0)
    if high > 1e-6:
        scale = peak / high
        for i in range(len(buf)):
            buf[i] *= scale


def write_wav(path: str, channels: list[list[float]], rate: int) -> int:
    """Writes 16-bit PCM. `channels` is one list for mono, two for stereo."""
    count = len(channels[0])
    interleaved = bytearray()
    for i in range(count):
        for ch in channels:
            v = ch[i]
            v = -1.0 if v < -1.0 else (1.0 if v > 1.0 else v)
            interleaved += struct.pack("<h", int(v * 32767.0))

    with wave.open(path, "wb") as w:
        w.setnchannels(len(channels))
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(bytes(interleaved))
    return os.path.getsize(path)


# ---------------------------------------------------------------------------
# One-shot effects
# ---------------------------------------------------------------------------


def sfx_weapon_fire(rng):
    buf = silence(0.12, SFX_RATE)
    square(buf, sweep(880.0, 220.0, 0.5), decay(4.0), SFX_RATE, duty=0.35)
    sine(buf, sweep(1760.0, 440.0, 0.5), lambda t: 0.3 * decay(5.0)(t), SFX_RATE)
    lowpass(buf, sweep(6000.0, 1200.0), SFX_RATE)
    return buf


def sfx_enemy_fire(rng):
    buf = silence(0.15, SFX_RATE)
    square(buf, sweep(300.0, 110.0, 0.6), decay(3.5), SFX_RATE, duty=0.5)
    noise(buf, lambda t: 0.15 * decay(6.0)(t), rng)
    lowpass(buf, sweep(2200.0, 500.0), SFX_RATE)
    return buf


def sfx_impact(rng):
    buf = silence(0.09, SFX_RATE)
    noise(buf, decay(6.0), rng)
    sine(buf, sweep(420.0, 160.0), lambda t: 0.5 * decay(5.0)(t), SFX_RATE)
    lowpass(buf, sweep(5000.0, 900.0), SFX_RATE)
    return buf


def sfx_explosion(rng):
    buf = silence(0.55, SFX_RATE)
    noise(buf, attack_decay(0.01, 2.5), rng)
    lowpass(buf, sweep(3000.0, 220.0, 0.6), SFX_RATE)
    sine(buf, sweep(160.0, 45.0, 0.7), lambda t: 0.7 * decay(2.5)(t), SFX_RATE)
    return buf


def sfx_big_explosion(rng):
    buf = silence(1.1, SFX_RATE)
    noise(buf, attack_decay(0.02, 1.8), rng)
    lowpass(buf, sweep(2400.0, 120.0, 0.5), SFX_RATE)
    sine(buf, sweep(110.0, 28.0, 0.8), lambda t: 0.9 * decay(1.6)(t), SFX_RATE)
    sine(buf, sweep(70.0, 20.0, 0.8), lambda t: 0.6 * decay(1.4)(t), SFX_RATE)
    return buf


def sfx_debris(rng):
    buf = silence(0.35, SFX_RATE)
    # A handful of discrete ticks rather than a wash, so it reads as fragments.
    for _ in range(7):
        start = rng.uniform(0.0, 0.75)
        tick = silence(0.05, SFX_RATE)
        noise(tick, decay(7.0), rng)
        highpass(tick, 1500.0, SFX_RATE)
        offset = frames(start * 0.35, SFX_RATE)
        for i, v in enumerate(tick):
            if offset + i < len(buf):
                buf[offset + i] += v * 0.5
    return buf


def _arpeggio(buf, notes, rate, note_len, amp=0.5, wave_fn=None):
    wave_fn = wave_fn or square
    for index, freq in enumerate(notes):
        seg = silence(note_len, rate)
        wave_fn(seg, const(freq), attack_decay(0.02, 3.0), rate)
        offset = frames(index * note_len * 0.8, rate)
        for i, v in enumerate(seg):
            if offset + i < len(buf):
                buf[offset + i] += v * amp


def sfx_powerup_pickup(rng):
    buf = silence(0.4, SFX_RATE)
    _arpeggio(buf, [523.25, 659.25, 783.99, 1046.50], SFX_RATE, 0.09, 0.45)
    lowpass(buf, const(6000.0), SFX_RATE)
    return buf


def sfx_cell_charge(rng):
    buf = silence(0.22, SFX_RATE)
    sine(buf, sweep(440.0, 880.0, 0.7), attack_decay(0.05, 3.0), SFX_RATE)
    sine(buf, sweep(660.0, 1320.0, 0.7), lambda t: 0.3 * attack_decay(0.05, 3.0)(t), SFX_RATE)
    return buf


def sfx_cell_break(rng):
    buf = silence(0.6, SFX_RATE)
    saw(buf, sweep(320.0, 60.0, 0.5), decay(2.2), SFX_RATE)
    noise(buf, lambda t: 0.35 * decay(3.0)(t), rng)
    lowpass(buf, sweep(3500.0, 300.0), SFX_RATE)
    return buf


def sfx_superweapon_charge(rng):
    buf = silence(0.7, SFX_RATE)
    saw(buf, sweep(120.0, 900.0, 1.6), attack_decay(0.1, 1.2), SFX_RATE)
    sine(buf, sweep(240.0, 1800.0, 1.6), lambda t: 0.4 * attack_decay(0.1, 1.2)(t), SFX_RATE)
    lowpass(buf, sweep(1200.0, 7000.0), SFX_RATE)
    return buf


def sfx_screen_clear(rng):
    buf = silence(1.4, SFX_RATE)
    # Rising whoosh into a low boom: the bomb reads as release, not impact.
    noise(buf, lambda t: 0.8 * (t ** 2) if t < 0.45 else 0.8 * decay(2.0)((t - 0.45) / 0.55), rng)
    lowpass(buf, lambda t: 400.0 + 6000.0 * min(1.0, t / 0.45), SFX_RATE)
    sine(buf, sweep(220.0, 35.0, 1.4), lambda t: 0.0 if t < 0.4 else 0.9 * decay(2.0)((t - 0.4) / 0.6), SFX_RATE)
    return buf


def sfx_graze(rng):
    # Very short and bright: fires many times a second under heavy fire, so it
    # has to sit on top of the mix without occupying it.
    buf = silence(0.05, SFX_RATE)
    sine(buf, sweep(2400.0, 1800.0), decay(5.0), SFX_RATE)
    highpass(buf, 1200.0, SFX_RATE)
    return buf


def sfx_secret_found(rng):
    buf = silence(0.9, SFX_RATE)
    _arpeggio(buf, [659.25, 783.99, 987.77, 1318.51, 1567.98], SFX_RATE, 0.13, 0.4,
              wave_fn=lambda b, f, a, r: sine(b, f, a, r))
    lowpass(buf, const(8000.0), SFX_RATE)
    return buf


def sfx_ui_move(rng):
    buf = silence(0.05, SFX_RATE)
    square(buf, const(660.0), decay(6.0), SFX_RATE, duty=0.25)
    return buf


def sfx_ui_select(rng):
    buf = silence(0.14, SFX_RATE)
    _arpeggio(buf, [880.0, 1174.66], SFX_RATE, 0.06, 0.5)
    return buf


def sfx_ui_back(rng):
    buf = silence(0.14, SFX_RATE)
    _arpeggio(buf, [587.33, 440.0], SFX_RATE, 0.06, 0.5)
    return buf


SFX_BUILDERS = {
    "weapon_fire": (sfx_weapon_fire, 0.55),
    "enemy_fire": (sfx_enemy_fire, 0.45),
    "impact": (sfx_impact, 0.5),
    "explosion": (sfx_explosion, 0.7),
    "big_explosion": (sfx_big_explosion, 0.85),
    "debris": (sfx_debris, 0.4),
    "powerup_pickup": (sfx_powerup_pickup, 0.7),
    "cell_charge": (sfx_cell_charge, 0.45),
    "cell_break": (sfx_cell_break, 0.8),
    "superweapon_charge": (sfx_superweapon_charge, 0.7),
    "screen_clear": (sfx_screen_clear, 0.9),
    "graze": (sfx_graze, 0.3),
    "secret_found": (sfx_secret_found, 0.8),
    "ui_move": (sfx_ui_move, 0.35),
    "ui_select": (sfx_ui_select, 0.5),
    "ui_back": (sfx_ui_back, 0.5),
}


# ---------------------------------------------------------------------------
# Music
#
# Three loops over a minor pentatonic, built from a bass line, an arpeggio and
# two drum voices. Tails that run past the end are wrapped back to the start so
# the loop point is seamless rather than a gap.
# ---------------------------------------------------------------------------

# A minor pentatonic, two octaves, as semitone offsets from A2.
PENTATONIC = [0, 3, 5, 7, 10, 12, 15, 17, 19, 22, 24]


def note_hz(semitones_from_a2: float) -> float:
    return 110.0 * (2.0 ** (semitones_from_a2 / 12.0))


def _mix_into(dst: list[float], src: list[float], offset: int, amp: float) -> None:
    """Adds src at offset, wrapping past the end back to the start so decay
    tails survive the loop point."""
    n = len(dst)
    for i, v in enumerate(src):
        dst[(offset + i) % n] += v * amp


def _kick(rate: int) -> list[float]:
    buf = silence(0.22, rate)
    sine(buf, sweep(150.0, 45.0, 0.4), decay(3.0), rate)
    return buf


def _hat(rate: int, rng: random.Random, length: float = 0.05) -> list[float]:
    buf = silence(length, rate)
    noise(buf, decay(8.0), rng)
    highpass(buf, 4000.0, rate)
    return buf


def build_music(name: str, seconds: float, bpm: float, seed: int,
                bass_octave: int, arp_density: int, drums: bool,
                rate: int = MUSIC_RATE):
    rng = random.Random(seed)
    total = frames(seconds, rate)
    left = [0.0] * total
    right = [0.0] * total

    beat = 60.0 / bpm
    step = beat / 2.0            # eighth notes
    steps = max(1, int(seconds / step))

    # A four-chord root movement, looping over the bar count.
    roots = [0, -2, 3, -4]

    for s in range(steps):
        t_off = frames(s * step, rate)
        bar = (s // 8) % len(roots)
        root = roots[bar] + bass_octave * 12

        # Bass on every other eighth.
        if s % 2 == 0:
            seg = silence(step * 1.8, rate)
            saw(seg, const(note_hz(root)), attack_decay(0.02, 2.0), rate)
            lowpass(seg, const(600.0), rate)
            _mix_into(left, seg, t_off, 0.35)
            _mix_into(right, seg, t_off, 0.35)

        # Arpeggio, panned slightly and alternating sides for width.
        if s % max(1, arp_density) == 0:
            degree = PENTATONIC[rng.randrange(3, len(PENTATONIC))]
            seg = silence(step * 1.5, rate)
            square(seg, const(note_hz(root + degree)), attack_decay(0.02, 3.0), rate, duty=0.3)
            lowpass(seg, const(3500.0), rate)
            side = 0.65 if (s // 2) % 2 == 0 else 0.35
            _mix_into(left, seg, t_off, 0.18 * side)
            _mix_into(right, seg, t_off, 0.18 * (1.0 - side))

        if drums:
            if s % 4 == 0:
                k = _kick(rate)
                _mix_into(left, k, t_off, 0.5)
                _mix_into(right, k, t_off, 0.5)
            if s % 2 == 1:
                h = _hat(rate, rng)
                _mix_into(left, h, t_off, 0.12)
                _mix_into(right, h, t_off, 0.12)

    highpass(left, 25.0, rate)
    highpass(right, 25.0, rate)
    normalise(left, 0.72)
    normalise(right, 0.72)
    # No declick: the loop is continuous by construction, and fading the edges
    # would put an audible dip at the loop point.
    return [left, right]


MUSIC_BUILDERS = {
    #                seed  bpm    bass  arp  drums  gain
    "music_menu":  (1701,  84.0,  -1,   4,   False, 0.45),
    "music_level": (1702, 132.0,   0,   2,   True,  0.5),
    "music_boss":  (1703, 152.0,   0,   1,   True,  0.55),
}


# ---------------------------------------------------------------------------
# Driver
# ---------------------------------------------------------------------------


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate placeholder audio.")
    parser.add_argument("--out", default="assets", help="output asset directory")
    parser.add_argument("--music-seconds", type=float, default=16.0,
                        help="length of each music loop (default 16)")
    parser.add_argument("--seed", type=int, default=20260801,
                        help="base RNG seed; output is reproducible for a given seed")
    args = parser.parse_args()

    out_dir = os.path.abspath(args.out)
    sound_dir = os.path.join(out_dir, "sounds")
    os.makedirs(sound_dir, exist_ok=True)

    manifest = {"sample_rate": SFX_RATE, "sounds": {}, "music": {}}
    total_bytes = 0

    print(f"writing to {sound_dir}")
    print("\neffects (mono, %d Hz):" % SFX_RATE)
    for index, name in enumerate(REQUIRED_SOUNDS):
        builder, gain = SFX_BUILDERS[name]
        rng = random.Random(args.seed + index)
        buf = builder(rng)
        # Asymmetric waveforms (a 25%-duty square, say) leave a DC bias that
        # thumps on every playback and eats headroom. Strip it below hearing
        # rather than fixing each generator, which would constrain the shapes
        # they are allowed to make.
        highpass(buf, 25.0, SFX_RATE)
        normalise(buf, 0.9)
        declick(buf, SFX_RATE)
        path = os.path.join(sound_dir, f"{name}.wav")
        size = write_wav(path, [buf], SFX_RATE)
        total_bytes += size
        manifest["sounds"][name] = {"file": f"sounds/{name}.wav", "gain": gain}
        print(f"  {name:<20} {len(buf) / SFX_RATE:5.2f}s  {size / 1024:7.1f} KB")

    print("\nmusic (stereo, %d Hz):" % MUSIC_RATE)
    for name in REQUIRED_MUSIC:
        seed, bpm, bass, arp, drums, gain = MUSIC_BUILDERS[name]
        channels = build_music(name, args.music_seconds, bpm, args.seed + seed,
                               bass, arp, drums)
        path = os.path.join(sound_dir, f"{name}.wav")
        size = write_wav(path, channels, MUSIC_RATE)
        total_bytes += size
        manifest["music"][name] = {"file": f"sounds/{name}.wav", "gain": gain, "loop": True}
        print(f"  {name:<20} {args.music_seconds:5.2f}s  {size / 1024:7.1f} KB  {bpm:.0f} bpm")

    manifest_path = os.path.join(out_dir, "sounds.json")
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, sort_keys=True)
        f.write("\n")

    print(f"\nwrote {manifest_path}")
    print(f"{len(REQUIRED_SOUNDS)} effects + {len(REQUIRED_MUSIC)} music, "
          f"{total_bytes / (1024 * 1024):.2f} MB total")
    return 0


if __name__ == "__main__":
    sys.exit(main())
