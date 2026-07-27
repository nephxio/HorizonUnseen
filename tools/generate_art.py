#!/usr/bin/env python3
"""
generate_art.py - bake all placeholder art for Horizon Unseen into one atlas.

Produces:
    <out>/atlas.png    RGBA, power-of-two, premultiplied-looking soft particles
    <out>/atlas.json   { image, width, height, sprites: { name: {x,y,w,h} } }

The sprite name list is a hard contract with src/Core/SpriteId.h. Every name in
REQUIRED below must exist in the JSON or the renderer logs a missing-sprite
warning at load time.

Everything is drawn from vector-ish primitives with a 4x supersample, then box
filtered down, so shapes stay legible without shipping any binary source art.
All randomness is seeded, so re-running byte-for-byte reproduces the output.

Usage:
    python tools/generate_art.py --out assets
    python tools/generate_art.py --out assets --size 2048
"""

from __future__ import annotations

import argparse
import json
import math
import os
import random
import sys

try:
    from PIL import Image, ImageChops, ImageDraw, ImageFilter
except ImportError:  # pragma: no cover
    sys.stderr.write(
        "error: Pillow is not installed.\n"
        "       python -m pip install --user -r tools/requirements.txt\n"
    )
    raise SystemExit(2)


# --------------------------------------------------------------------------
# Contract: these names must all end up as keys in atlas.json.
# Keep in sync with spriteName() in src/Core/SpriteId.h.
# --------------------------------------------------------------------------
REQUIRED = [
    "ship_player", "ship_player_bank_up", "ship_player_bank_down",

    "enemy_drifter", "enemy_waverider", "enemy_diver", "enemy_turret",
    "enemy_splitter", "enemy_splitter_child", "enemy_orbiter", "enemy_mine",
    "enemy_boss",

    "bullet_player", "missile_player", "lance_player",
    "laser_segment", "laser_head",

    "bullet_enemy", "bullet_enemy_heavy", "missile_enemy",

    "powerup_spread", "powerup_missile", "powerup_laser", "powerup_cannon",
    "powerup_repair", "powerup_energy",

    "particle_spark", "particle_glow", "particle_smoke", "particle_ring",
    "particle_shard",

    "star_small", "star_large", "nebula_patch",

    "white",
]

SEED = 0x48555F31  # "HU_1"
SS = 4             # supersample factor
PAD = 2            # transparent gutter between packed sprites


# --------------------------------------------------------------------------
# Small drawing toolkit
# --------------------------------------------------------------------------

def _sc(pts, s):
    return [(x * s, y * s) for (x, y) in pts]


class Sheet:
    """An RGBA sprite canvas drawn at SS scale and downsampled on finish()."""

    def __init__(self, w, h, s=SS):
        self.w, self.h, self.s = w, h, s
        self.img = Image.new("RGBA", (w * s, h * s), (0, 0, 0, 0))
        self.d = ImageDraw.Draw(self.img, "RGBA")

    # -- primitives (coordinates are in sprite space, floats welcome) --
    def poly(self, pts, fill=None, outline=None, width=1.0):
        self.d.polygon(_sc(pts, self.s), fill=fill, outline=outline,
                       width=max(1, int(round(width * self.s))))

    def ellipse(self, box, fill=None, outline=None, width=1.0):
        x0, y0, x1, y1 = box
        self.d.ellipse([x0 * self.s, y0 * self.s, x1 * self.s, y1 * self.s],
                       fill=fill, outline=outline,
                       width=max(1, int(round(width * self.s))))

    def circle(self, cx, cy, r, fill=None, outline=None, width=1.0):
        self.ellipse((cx - r, cy - r, cx + r, cy + r), fill, outline, width)

    def line(self, pts, fill, width=1.0):
        self.d.line(_sc(pts, self.s), fill=fill,
                    width=max(1, int(round(width * self.s))), joint="curve")

    def rect(self, box, fill=None, outline=None, width=1.0):
        x0, y0, x1, y1 = box
        self.d.rectangle([x0 * self.s, y0 * self.s, x1 * self.s, y1 * self.s],
                         fill=fill, outline=outline,
                         width=max(1, int(round(width * self.s))))

    def round_rect(self, box, r, fill=None, outline=None, width=1.0):
        x0, y0, x1, y1 = box
        self.d.rounded_rectangle(
            [x0 * self.s, y0 * self.s, x1 * self.s, y1 * self.s],
            radius=r * self.s, fill=fill, outline=outline,
            width=max(1, int(round(width * self.s))))

    def arc(self, box, a0, a1, fill, width=1.0):
        x0, y0, x1, y1 = box
        self.d.arc([x0 * self.s, y0 * self.s, x1 * self.s, y1 * self.s],
                   a0, a1, fill=fill,
                   width=max(1, int(round(width * self.s))))

    # -- shaded fills --
    def grad_poly(self, pts, top, bottom):
        """Fill a polygon with a vertical linear gradient."""
        ys = [p[1] for p in pts]
        y0, y1 = min(ys), max(ys)
        mask = Image.new("L", self.img.size, 0)
        ImageDraw.Draw(mask).polygon(_sc(pts, self.s), fill=255)
        self.img.paste(self._vgrad(top, bottom, y0, y1), (0, 0), mask)

    def grad_ellipse(self, box, top, bottom):
        x0, y0, x1, y1 = box
        mask = Image.new("L", self.img.size, 0)
        ImageDraw.Draw(mask).ellipse(
            [x0 * self.s, y0 * self.s, x1 * self.s, y1 * self.s], fill=255)
        self.img.paste(self._vgrad(top, bottom, y0, y1), (0, 0), mask)

    def _vgrad(self, top, bottom, y0, y1):
        W, H = self.img.size
        g = Image.new("RGBA", (1, H))
        px = g.load()
        a = max(1e-6, (y1 - y0) * self.s)
        for y in range(H):
            t = min(1.0, max(0.0, (y - y0 * self.s) / a))
            px[0, y] = tuple(int(round(top[i] + (bottom[i] - top[i]) * t))
                             for i in range(4))
        return g.resize((W, H), Image.NEAREST)

    def finish(self):
        return self.img.resize((self.w, self.h), Image.LANCZOS)


def radial(w, h, cx, cy, r, color, power=2.0, inner=0.0):
    """Soft radial falloff, bright core fading to transparent. Additive-safe."""
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = img.load()
    cr, cg, cb = color
    for y in range(h):
        dy = (y + 0.5) - cy
        for x in range(w):
            dx = (x + 0.5) - cx
            d = math.hypot(dx, dy) / r
            if d >= 1.0:
                continue
            t = 1.0 if d <= inner else (1.0 - (d - inner) / max(1e-6, 1.0 - inner))
            t = t ** power
            # premultiplied-looking: colour dims with the alpha ramp too
            k = 0.35 + 0.65 * t
            px[x, y] = (int(cr * k), int(cg * k), int(cb * k), int(255 * t))
    return img


def ring(w, h, cx, cy, r, thick, color, power=2.0):
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    px = img.load()
    cr, cg, cb = color
    for y in range(h):
        dy = (y + 0.5) - cy
        for x in range(w):
            dx = (x + 0.5) - cx
            d = abs(math.hypot(dx, dy) - r) / thick
            if d >= 1.0:
                continue
            t = (1.0 - d) ** power
            k = 0.4 + 0.6 * t
            px[x, y] = (int(cr * k), int(cg * k), int(cb * k), int(255 * t))
    return img


def _smooth(t):
    return t * t * (3.0 - 2.0 * t)


def value_noise(w, h, cells, rng, octaves=4):
    """Seeded fractal value noise in [0,1], returned as a flat float list."""
    out = [0.0] * (w * h)
    amp, total = 1.0, 0.0
    for o in range(octaves):
        n = cells * (2 ** o)
        grid = [[rng.random() for _ in range(n + 1)] for _ in range(n + 1)]
        for y in range(h):
            gy = (y / h) * n
            iy = int(gy)
            fy = _smooth(gy - iy)
            for x in range(w):
                gx = (x / w) * n
                ix = int(gx)
                fx = _smooth(gx - ix)
                a = grid[iy][ix] + (grid[iy][ix + 1] - grid[iy][ix]) * fx
                b = grid[iy + 1][ix] + (grid[iy + 1][ix + 1] - grid[iy + 1][ix]) * fx
                out[y * w + x] += (a + (b - a) * fy) * amp
        total += amp
        amp *= 0.5
    return [v / total for v in out]


def over(base, top):
    return Image.alpha_composite(base, top)


def add(base, top):
    """Additive-ish composite used for engine glows and hot cores."""
    b = base.copy()
    lit = ImageChops.add(b.convert("RGB"), top.convert("RGB"))
    a = ImageChops.lighter(b.getchannel("A"), top.getchannel("A"))
    lit.putalpha(a)
    return lit


def glow_blob(w, h, cx, cy, r, color, power=2.2):
    return radial(w, h, cx, cy, r, color, power)


# --------------------------------------------------------------------------
# Player ship
# --------------------------------------------------------------------------

def make_player(bank=0):
    """bank: 0 level, +1 nose up, -1 nose down. Rolled variants foreshorten
    the wings and shear the hull so the tilt reads without a real 3D model."""
    W, H = 64, 40
    cy, cx = 20.0, 32.0
    shear = -0.17 * bank        # screen y grows downward
    wing_k = 1.0 if bank == 0 else 0.52

    def T(x, y, wing=False):
        if wing:
            y = cy + (y - cy) * wing_k
        return (x, y + (x - cx) * shear)

    s = Sheet(W, H)

    # wings (behind the hull)
    up_wing = [T(36, 15, True), T(18, 3, True), T(6, 4, True),
               T(10, 12, True), T(24, 18, True)]
    dn_wing = [T(36, 25, True), T(18, 37, True), T(6, 36, True),
               T(10, 28, True), T(24, 22, True)]
    s.grad_poly(up_wing, (96, 132, 190, 255), (44, 66, 110, 255))
    s.grad_poly(dn_wing, (52, 78, 128, 255), (30, 46, 80, 255))
    s.poly(up_wing, outline=(176, 210, 255, 230), width=0.8)
    s.poly(dn_wing, outline=(140, 176, 226, 220), width=0.8)

    # main hull
    hull = [T(62, 20), T(46, 13), T(26, 10), T(9, 13),
            T(4, 20), T(9, 27), T(26, 30), T(46, 27)]
    s.grad_poly(hull, (232, 240, 252, 255), (58, 84, 132, 255))
    s.poly(hull, outline=(206, 228, 255, 255), width=0.9)

    # spine highlight + panel lines
    s.line([T(52, 18.5), T(12, 16.5)], (255, 255, 255, 140), 0.9)
    s.line([T(48, 24.5), T(14, 25.0)], (18, 30, 54, 150), 0.7)
    s.line([T(30, 11.5), T(30, 28.5)], (24, 40, 70, 120), 0.6)
    s.line([T(20, 12.5), T(20, 29.0)], (24, 40, 70, 100), 0.6)

    # canopy
    cpy = [T(48, 20), T(40, 16), T(31, 16.5), T(29, 20), T(32, 23.5), T(42, 23)]
    s.grad_poly(cpy, (150, 232, 255, 255), (26, 74, 122, 255))
    s.poly(cpy, outline=(214, 248, 255, 240), width=0.7)
    s.line([T(44, 18.4), T(34, 18.0)], (255, 255, 255, 210), 0.7)

    # nose strake / chin cannon
    s.poly([T(63, 20), T(52, 18.2), T(52, 21.8)], fill=(255, 226, 160, 255))

    # tail fins
    s.poly([T(14, 14), T(6, 6), T(3, 9), T(10, 16)], fill=(196, 216, 244, 235))
    s.poly([T(14, 26), T(6, 34), T(3, 31), T(10, 24)], fill=(150, 176, 214, 235))

    img = s.finish()

    # engine bloom, additive
    g = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    for (ex, ey, r, col) in ((7.5, 16.5, 7.5, (110, 200, 255)),
                             (7.5, 23.5, 7.5, (110, 200, 255)),
                             (5.0, 20.0, 5.0, (200, 240, 255))):
        ex_, ey_ = T(ex, ey)
        g = add(g, glow_blob(W, H, ex_, ey_, r, col, 2.6))
    return add(img, g)


# --------------------------------------------------------------------------
# Enemies (all nose-left unless noted)
# --------------------------------------------------------------------------

def make_drifter():
    """Grey blunt wedge. Slow, dumb, everywhere."""
    W, H = 48, 32
    s = Sheet(W, H)
    body = [(2, 16), (16, 5), (40, 6), (46, 12), (46, 20), (40, 26), (16, 27)]
    s.grad_poly(body, (176, 182, 190, 255), (66, 70, 78, 255))
    s.poly(body, outline=(214, 220, 228, 255), width=0.9)
    s.line([(16, 6.5), (16, 26.5)], (40, 44, 52, 190), 0.8)
    s.line([(28, 5.8), (28, 26.8)], (40, 44, 52, 150), 0.7)
    s.line([(18, 10.5), (42, 11.0)], (236, 240, 246, 120), 0.7)
    # armoured brow + sensor eye
    s.poly([(4, 16), (15, 9), (15, 23)], fill=(112, 118, 128, 255))
    s.ellipse((8, 13.5, 15, 18.5), fill=(255, 120, 96, 255))
    s.ellipse((9.5, 14.5, 13, 16.6), fill=(255, 214, 200, 220))
    # rear thruster block
    s.rect((41, 10, 46, 22), fill=(52, 56, 64, 255))
    img = s.finish()
    g = add(Image.new("RGBA", (W, H), (0, 0, 0, 0)),
            glow_blob(W, H, 46, 16, 8, (170, 120, 70), 2.6))
    return add(img, g)


def make_waverider():
    """Cyan swept-wing interceptor - wide, thin, fast-looking."""
    W, H = 52, 36
    s = Sheet(W, H)
    up = [(14, 17), (34, 2), (48, 4), (30, 15)]
    dn = [(14, 19), (34, 34), (48, 32), (30, 21)]
    s.grad_poly(up, (86, 236, 250, 255), (16, 108, 134, 255))
    s.grad_poly(dn, (18, 120, 148, 255), (10, 70, 92, 255))
    s.poly(up, outline=(190, 252, 255, 235), width=0.8)
    s.poly(dn, outline=(150, 236, 250, 220), width=0.8)
    body = [(1, 18), (12, 12), (40, 13), (48, 18), (40, 23), (12, 24)]
    s.grad_poly(body, (206, 250, 255, 255), (14, 92, 120, 255))
    s.poly(body, outline=(200, 250, 255, 255), width=0.9)
    s.line([(14, 15.5), (40, 15.8)], (255, 255, 255, 150), 0.7)
    s.poly([(1, 18), (11, 14.5), (11, 21.5)], fill=(255, 244, 176, 255))
    s.ellipse((22, 15.4, 33, 20.6), fill=(10, 46, 62, 255))
    s.ellipse((23.5, 16.4, 30, 18.6), fill=(120, 226, 255, 200))
    img = s.finish()
    g = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    for (ex, ey) in ((47, 18), (45, 6), (45, 30)):
        g = add(g, glow_blob(W, H, ex, ey, 6.5, (60, 190, 220), 2.6))
    return add(img, g)


def make_diver():
    """Red dart. Narrow, aggressive, all nose."""
    W, H = 44, 28
    s = Sheet(W, H)
    fin_u = [(30, 13), (42, 2), (44, 7), (34, 14)]
    fin_d = [(30, 15), (42, 26), (44, 21), (34, 14)]
    s.grad_poly(fin_u, (255, 120, 96, 255), (128, 26, 26, 255))
    s.grad_poly(fin_d, (168, 40, 38, 255), (94, 18, 18, 255))
    body = [(0, 14), (10, 9), (34, 8), (43, 14), (34, 20), (10, 19)]
    s.grad_poly(body, (255, 168, 132, 255), (122, 22, 24, 255))
    s.poly(body, outline=(255, 190, 160, 255), width=0.9)
    s.line([(12, 11.4), (34, 11.0)], (255, 232, 214, 160), 0.7)
    s.line([(12, 17.4), (34, 17.6)], (60, 10, 12, 160), 0.7)
    s.poly([(0, 14), (12, 10.8), (12, 17.2)], fill=(255, 240, 214, 255))
    s.ellipse((16, 12, 26, 16.4), fill=(48, 8, 10, 255))
    s.ellipse((17.5, 12.7, 22.5, 14.6), fill=(255, 160, 140, 200))
    img = s.finish()
    g = add(Image.new("RGBA", (W, H), (0, 0, 0, 0)),
            glow_blob(W, H, 43, 14, 7.5, (220, 90, 60), 2.4))
    return add(img, g)


def make_turret():
    """Armoured bunker with a rotating dome and a barrel pointing left."""
    W, H = 48, 40
    s = Sheet(W, H)
    base = [(4, 39), (44, 39), (40, 26), (8, 26)]
    s.grad_poly(base, (128, 126, 112, 255), (46, 46, 42, 255))
    s.poly(base, outline=(168, 166, 150, 255), width=0.9)
    for bx in (14, 22, 30, 38):
        s.line([(bx, 26.5), (bx - 1.5, 38.5)], (34, 34, 30, 170), 0.8)
    s.rect((6, 24, 42, 27), fill=(158, 156, 140, 255))
    # dome
    s.grad_ellipse((12, 8, 40, 28), (198, 196, 178, 255), (72, 72, 64, 255))
    s.arc((12, 8, 40, 28), 180, 360, (226, 224, 206, 255), 1.0)
    s.arc((16, 11, 36, 25), 190, 350, (255, 255, 240, 110), 0.8)
    # barrel to the left
    s.rect((1, 15, 16, 21), fill=(58, 58, 54, 255))
    s.rect((1, 15, 16, 17), fill=(112, 112, 104, 255))
    s.rect((0, 14, 5, 22), fill=(38, 38, 36, 255))
    # hazard stripe + eye
    s.rect((14, 21, 38, 24), fill=(226, 176, 40, 255))
    for hx in range(15, 38, 5):
        s.poly([(hx, 21), (hx + 2.5, 21), (hx - 0.5, 24), (hx - 3, 24)],
               fill=(40, 36, 24, 220))
    s.circle(26, 15.5, 3.4, fill=(255, 96, 72, 255))
    img = s.finish()
    g = add(Image.new("RGBA", (W, H), (0, 0, 0, 0)),
            glow_blob(W, H, 26, 15.5, 7.0, (170, 50, 34), 2.6))
    return add(img, g)


def _splitter_body(W, H, lobes, rng):
    s = Sheet(W, H)
    cx, cy = W * 0.5, H * 0.5
    for i, (ox, oy, r) in enumerate(lobes):
        s.grad_ellipse((cx + ox - r, cy + oy - r, cx + ox + r, cy + oy + r),
                       (170, 244, 128, 255), (28, 96, 44, 255))
        s.ellipse((cx + ox - r, cy + oy - r, cx + ox + r, cy + oy + r),
                  outline=(214, 255, 176, 230), width=0.8)
    # membrane seams between lobes
    for (ox, oy, r) in lobes:
        s.arc((cx + ox - r * 0.62, cy + oy - r * 0.62,
               cx + ox + r * 0.62, cy + oy + r * 0.62),
              0, 360, (18, 68, 30, 150), 0.7)
    # nucleus
    nr = min(W, H) * 0.16
    s.circle(cx - W * 0.08, cy, nr, fill=(250, 255, 210, 255))
    s.circle(cx - W * 0.08, cy, nr * 0.55, fill=(96, 196, 72, 255))
    # a few speckles, seeded
    for _ in range(int(W * H / 90)):
        a = rng.random() * math.tau
        d = math.sqrt(rng.random()) * min(W, H) * 0.36
        px, py = cx + math.cos(a) * d, cy + math.sin(a) * d
        s.circle(px, py, 0.9, fill=(24, 88, 38, 150))
    return s.finish()


def make_splitter(rng):
    W, H = 44, 44
    lobes = [(-4, 0, 15), (8, -9, 10), (8, 9, 10), (12, 0, 8)]
    return _splitter_body(W, H, lobes, rng)


def make_splitter_child(rng):
    W, H = 24, 24
    lobes = [(-2, 0, 8.5), (4, -4, 5.5), (4, 4, 5.5)]
    return _splitter_body(W, H, lobes, rng)


def make_orbiter():
    """Purple ring-ship: a torus with a small core inside."""
    W, H = 48, 48
    s = Sheet(W, H)
    cx = cy = 24.0
    s.circle(cx, cy, 22.5, outline=(126, 78, 220, 255), width=5.0)
    s.circle(cx, cy, 20.5, outline=(196, 156, 255, 235), width=1.6)
    s.circle(cx, cy, 24.0, outline=(72, 34, 140, 230), width=1.2)
    # four pylons + node pods on the ring
    for a in (35, 145, 215, 325):
        r = math.radians(a)
        s.line([(cx + math.cos(r) * 7, cy + math.sin(r) * 7),
                (cx + math.cos(r) * 21, cy + math.sin(r) * 21)],
               (168, 130, 244, 255), 2.2)
        s.circle(cx + math.cos(r) * 22, cy + math.sin(r) * 22, 3.4,
                 fill=(226, 200, 255, 255))
    # core
    s.grad_ellipse((cx - 8, cy - 8, cx + 8, cy + 8),
                   (236, 214, 255, 255), (86, 40, 160, 255))
    s.circle(cx, cy, 4.2, fill=(255, 244, 210, 255))
    # forward (left) prow so facing reads
    s.poly([(1, 24), (10, 20), (10, 28)], fill=(214, 186, 255, 255))
    img = s.finish()
    g = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    g = add(g, glow_blob(W, H, cx, cy, 12, (150, 90, 230), 2.4))
    for a in (35, 145, 215, 325):
        r = math.radians(a)
        g = add(g, glow_blob(W, H, cx + math.cos(r) * 22, cy + math.sin(r) * 22,
                             6, (120, 70, 200), 2.6))
    return add(img, g)


def make_mine():
    """Spiked orange sphere. No facing - it just sits there and hates you."""
    W, H = 36, 36
    s = Sheet(W, H)
    cx = cy = 18.0
    for i in range(12):
        a = math.radians(i * 30 + 15)
        tip = (cx + math.cos(a) * 17.5, cy + math.sin(a) * 17.5)
        b0 = (cx + math.cos(a - 0.30) * 11.5, cy + math.sin(a - 0.30) * 11.5)
        b1 = (cx + math.cos(a + 0.30) * 11.5, cy + math.sin(a + 0.30) * 11.5)
        s.poly([tip, b0, b1], fill=(196, 108, 24, 255))
        s.line([b0, tip], (255, 196, 110, 220), 0.6)
    s.grad_ellipse((cx - 12, cy - 12, cx + 12, cy + 12),
                   (255, 186, 90, 255), (128, 58, 12, 255))
    s.circle(cx, cy, 12, outline=(255, 214, 150, 255), width=1.0)
    s.arc((cx - 12, cy - 12, cx + 12, cy + 12), 200, 340, (60, 24, 6, 160), 1.4)
    s.circle(cx - 3.5, cy - 4, 3.2, fill=(255, 236, 196, 130))
    s.circle(cx, cy, 4.0, fill=(60, 22, 8, 255))
    s.circle(cx, cy, 2.4, fill=(255, 90, 40, 255))
    img = s.finish()
    g = add(Image.new("RGBA", (W, H), (0, 0, 0, 0)),
            glow_blob(W, H, cx, cy, 8, (210, 70, 20), 2.8))
    return add(img, g)


def make_boss():
    """192x160 dark multi-part hull, red accents, big left-facing prow."""
    W, H = 192, 160
    s = Sheet(W, H)
    cy = 80.0

    # rear engine block
    s.grad_poly([(150, 26), (190, 40), (190, 120), (150, 134)],
                (78, 82, 96, 255), (20, 22, 30, 255))
    for y in (48, 80, 112):
        s.rect((168, y - 9, 190, y + 9), fill=(34, 36, 46, 255))
        s.rect((170, y - 6, 188, y + 6), fill=(226, 96, 60, 255))

    # upper and lower wing slabs
    up = [(60, 44), (150, 18), (176, 26), (150, 52), (74, 58)]
    dn = [(60, 116), (150, 142), (176, 134), (150, 108), (74, 102)]
    s.grad_poly(up, (92, 98, 114, 255), (30, 32, 42, 255))
    s.grad_poly(dn, (46, 50, 62, 255), (22, 24, 32, 255))
    s.poly(up, outline=(132, 140, 158, 240), width=1.2)
    s.poly(dn, outline=(110, 118, 136, 230), width=1.2)
    for i in range(5):
        x = 78 + i * 18
        s.line([(x, 24 + i * 2.5), (x + 6, 54)], (16, 18, 24, 170), 1.0)
        s.line([(x, 136 - i * 2.5), (x + 6, 106)], (16, 18, 24, 150), 1.0)

    # main hull
    hull = [(6, 80), (34, 44), (78, 28), (150, 34), (168, 60),
            (168, 100), (150, 126), (78, 132), (34, 116)]
    s.grad_poly(hull, (118, 124, 142, 255), (24, 26, 36, 255))
    s.poly(hull, outline=(158, 166, 186, 255), width=1.4)
    s.line([(40, 52), (150, 44)], (198, 206, 226, 90), 1.2)
    s.line([(40, 108), (150, 116)], (10, 12, 18, 150), 1.2)
    for x in (70, 100, 130):
        s.line([(x, 32), (x, 128)], (14, 16, 22, 120), 1.0)

    # armoured prow
    prow = [(2, 80), (30, 56), (58, 66), (58, 94), (30, 104)]
    s.grad_poly(prow, (176, 182, 200, 255), (44, 46, 58, 255))
    s.poly(prow, outline=(210, 216, 232, 255), width=1.2)
    s.poly([(2, 80), (26, 68), (26, 92)], fill=(236, 88, 62, 255))

    # red accent spine and shoulder stripes
    s.poly([(58, 74), (140, 70), (150, 80), (140, 90), (58, 86)],
           fill=(28, 30, 40, 255))
    s.poly([(62, 76), (138, 73), (146, 80), (138, 87), (62, 84)],
           fill=(230, 62, 48, 255))
    for (sx, sy) in ((92, 40), (120, 38), (92, 120), (120, 122)):
        s.round_rect((sx - 10, sy - 5, sx + 10, sy + 5), 2.5,
                     fill=(198, 54, 44, 235))

    # weapon pods and core eye
    for (px_, py_) in ((52, 44), (52, 116)):
        s.round_rect((px_ - 12, py_ - 9, px_ + 12, py_ + 9), 4,
                     fill=(58, 62, 76, 255), outline=(120, 128, 146, 255),
                     width=1.0)
        s.rect((px_ - 22, py_ - 3, px_ - 10, py_ + 3), fill=(36, 38, 48, 255))
        s.circle(px_ + 4, py_, 3.2, fill=(255, 132, 96, 255))
    s.grad_ellipse((66, 62, 106, 98), (255, 214, 190, 255), (150, 22, 18, 255))
    s.circle(86, 80, 9, fill=(255, 96, 64, 255))
    s.circle(86, 80, 4, fill=(255, 244, 220, 255))

    img = s.finish()
    g = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    g = add(g, glow_blob(W, H, 86, 80, 26, (190, 40, 26), 2.6))
    for y in (48, 80, 112):
        g = add(g, glow_blob(W, H, 190, y, 20, (170, 70, 40), 2.4))
    for (px_, py_) in ((56, 44), (56, 116)):
        g = add(g, glow_blob(W, H, px_, py_, 10, (150, 60, 40), 2.6))
    return add(img, g)


# --------------------------------------------------------------------------
# Projectiles
# --------------------------------------------------------------------------

def make_bullet_player():
    W, H = 16, 8
    s = Sheet(W, H)
    s.poly([(15, 4), (6, 1), (1, 4), (6, 7)], fill=(120, 210, 255, 220))
    s.poly([(14, 4), (7, 2.2), (3, 4), (7, 5.8)], fill=(232, 250, 255, 255))
    img = s.finish()
    g = add(Image.new("RGBA", (W, H), (0, 0, 0, 0)),
            glow_blob(W, H, 8, 4, 8, (70, 150, 220), 2.2))
    return add(img, g)


def make_missile(player=True):
    W, H = 24, 10
    s = Sheet(W, H)
    if player:
        hull_top, hull_bot = (238, 244, 252, 255), (96, 116, 150, 255)
        acc, flame = (86, 176, 255, 255), (110, 200, 255)
    else:
        hull_top, hull_bot = (226, 172, 160, 255), (94, 40, 38, 255)
        acc, flame = (240, 96, 64, 255), (230, 110, 60)
    body = [(22, 5), (16, 2), (5, 2), (4, 5), (5, 8), (16, 8)]
    s.grad_poly(body, hull_top, hull_bot)
    s.poly(body, outline=(255, 255, 255, 170), width=0.6)
    s.poly([(23, 5), (17, 2.6), (17, 7.4)], fill=acc)
    s.poly([(8, 2), (3, 0), (2, 3), (7, 3.4)], fill=acc)
    s.poly([(8, 8), (3, 10), (2, 7), (7, 6.6)], fill=acc)
    s.line([(16, 4.2), (6, 4.2)], (255, 255, 255, 150), 0.6)
    img = s.finish()
    g = add(Image.new("RGBA", (W, H), (0, 0, 0, 0)),
            glow_blob(W, H, 2.5, 5, 7, flame, 2.4))
    return add(img, g)


def make_lance():
    """Piercing spear round: long, thin, white-hot."""
    W, H = 32, 8
    s = Sheet(W, H)
    s.poly([(31, 4), (10, 0.6), (0, 4), (10, 7.4)], fill=(150, 120, 255, 190))
    s.poly([(30, 4), (12, 2.0), (3, 4), (12, 6.0)], fill=(212, 190, 255, 245))
    s.poly([(29, 4), (14, 3.0), (7, 4), (14, 5.0)], fill=(255, 255, 255, 255))
    img = s.finish()
    g = add(Image.new("RGBA", (W, H), (0, 0, 0, 0)),
            glow_blob(W, H, 18, 4, 14, (100, 70, 200), 2.0))
    return add(img, g)


def make_laser_segment():
    """Tileable beam cross-section: constant along X, so any tiling works."""
    W, H = 16, 24
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    px = img.load()
    cy = H / 2.0
    for y in range(H):
        d = abs((y + 0.5) - cy) / (H / 2.0)
        core = max(0.0, 1.0 - (d / 0.18)) ** 1.5
        halo = max(0.0, 1.0 - d) ** 2.4
        a = min(1.0, core + halo * 0.85)
        r = 90 + 165 * core
        g = 210 + 45 * core
        b = 255
        k = 0.4 + 0.6 * a
        row = (int(r * k), int(g * k), int(b * k), int(255 * a))
        for x in range(W):
            px[x, y] = row
    return img


def make_laser_head():
    """Leading cap for the beam: same cross-section, rounded off to the right."""
    W, H = 20, 24
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    px = img.load()
    cy, cx = H / 2.0, 4.0
    for y in range(H):
        for x in range(W):
            dy = abs((y + 0.5) - cy) / (H / 2.0)
            dx = max(0.0, ((x + 0.5) - cx) / (W - cx))
            d = math.hypot(dy, dx * 1.05)
            if d >= 1.0:
                continue
            core = max(0.0, 1.0 - (d / 0.30)) ** 1.4
            halo = max(0.0, 1.0 - d) ** 2.2
            a = min(1.0, core + halo * 0.9)
            k = 0.4 + 0.6 * a
            px[x, y] = (int((110 + 145 * core) * k),
                        int((215 + 40 * core) * k),
                        int(255 * k), int(255 * a))
    return img


def make_bullet_enemy(heavy=False):
    if heavy:
        W = H = 20
        core, edge, halo = (255, 236, 200), (232, 70, 40, 255), (200, 50, 24)
    else:
        W = H = 12
        core, edge, halo = (255, 214, 236), (226, 64, 148, 255), (180, 40, 120)
    s = Sheet(W, H)
    c = W / 2.0
    s.circle(c, c, c - 0.6, fill=edge)
    s.grad_ellipse((1.6, 1.6, W - 1.6, H - 1.6),
                   (255, 250, 240, 255), edge)
    s.circle(c, c, c * 0.42, fill=core + (255,))
    if heavy:
        for i in range(6):
            a = math.radians(i * 60)
            s.line([(c + math.cos(a) * c * 0.55, c + math.sin(a) * c * 0.55),
                    (c + math.cos(a) * (c - 1.0), c + math.sin(a) * (c - 1.0))],
                   (120, 16, 8, 200), 1.0)
    img = s.finish()
    g = add(Image.new("RGBA", (W, H), (0, 0, 0, 0)),
            glow_blob(W, H, c, c, c, halo, 2.2))
    return add(img, g)


# --------------------------------------------------------------------------
# Power-ups
# --------------------------------------------------------------------------

def _capsule(icon, base, light, W=28, H=28):
    s = Sheet(W, H)
    s.round_rect((1, 1, W - 1, H - 1), 7, fill=(14, 16, 24, 235))
    s.round_rect((1, 1, W - 1, H - 1), 7, outline=light, width=1.4)
    # inner shaded pill
    mask = Image.new("L", s.img.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        [3 * SS, 3 * SS, (W - 3) * SS, (H - 3) * SS], radius=5 * SS, fill=255)
    s.img.paste(s._vgrad(light, base, 3, H - 3), (0, 0), mask)
    # gloss
    s.round_rect((4, 4, W - 4, H * 0.44), 4, fill=(255, 255, 255, 46))
    icon(s, W, H)
    img = s.finish()
    g = add(Image.new("RGBA", (W, H), (0, 0, 0, 0)),
            glow_blob(W, H, W / 2, H / 2, W * 0.62, tuple(c // 3 for c in base[:3]), 2.4))
    return add(img, g)


def make_powerups():
    out = {}
    ink = (12, 14, 22, 255)

    def spread(s, W, H):
        cx, cy = 9.0, H / 2
        for a in (-26, 0, 26):
            r = math.radians(a)
            s.line([(cx, cy), (cx + math.cos(r) * 12, cy + math.sin(r) * 12)],
                   ink, 1.8)
            s.poly([(cx + math.cos(r) * 14, cy + math.sin(r) * 14),
                    (cx + math.cos(r) * 9.5 - math.sin(r) * 2.6,
                     cy + math.sin(r) * 9.5 + math.cos(r) * 2.6),
                    (cx + math.cos(r) * 9.5 + math.sin(r) * 2.6,
                     cy + math.sin(r) * 9.5 - math.cos(r) * 2.6)], fill=ink)

    def missile(s, W, H):
        s.poly([(22, 14), (14, 9), (7, 9), (6, 14), (7, 19), (14, 19)], fill=ink)
        s.poly([(11, 9), (6, 5), (5, 9)], fill=ink)
        s.poly([(11, 19), (6, 23), (5, 19)], fill=ink)
        s.poly([(7, 11), (3, 14), (7, 17)], fill=(255, 226, 150, 255))

    def laser(s, W, H):
        s.poly([(4, 12), (22, 12), (22, 16), (4, 16)], fill=ink)
        s.poly([(22, 10), (26, 14), (22, 18)], fill=ink)
        s.rect((6, 13.2, 20, 14.8), fill=(255, 255, 255, 235))

    def cannon(s, W, H):
        s.round_rect((5, 10, 21, 18), 2.5, fill=ink)
        s.rect((19, 11.5, 25, 16.5), fill=ink)
        s.rect((3, 8.5, 8, 19.5), fill=ink)
        s.circle(13, 14, 3.0, fill=(255, 236, 190, 255))

    def repair(s, W, H):
        s.round_rect((11, 5, 17, 23), 1.6, fill=ink)
        s.round_rect((5, 11, 23, 17), 1.6, fill=ink)
        s.round_rect((12.4, 6.4, 15.6, 21.6), 1.0, fill=(255, 255, 255, 140))

    def energy(s, W, H):
        s.poly([(17, 4), (9, 15), (13.5, 15), (11, 24), (20, 12),
                (15, 12), (17, 4)], fill=ink)
        s.poly([(16, 7), (11.5, 14), (14, 14)], fill=(255, 255, 255, 150))

    out["powerup_spread"] = _capsule(spread, (24, 108, 196, 255), (140, 216, 255, 255))
    out["powerup_missile"] = _capsule(missile, (170, 70, 20, 255), (255, 190, 120, 255))
    out["powerup_laser"] = _capsule(laser, (150, 24, 120, 255), (255, 150, 236, 255))
    out["powerup_cannon"] = _capsule(cannon, (140, 118, 22, 255), (255, 232, 130, 255))
    out["powerup_repair"] = _capsule(repair, (20, 138, 72, 255), (150, 255, 190, 255))
    out["powerup_energy"] = _capsule(energy, (96, 46, 190, 255), (200, 176, 255, 255))
    return out


# --------------------------------------------------------------------------
# Particles (drawn additively in-engine: bright on transparent black)
# --------------------------------------------------------------------------

def make_particle_spark():
    W, H = 16, 8
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    px = img.load()
    cy = H / 2.0
    for y in range(H):
        for x in range(W):
            u = (x + 0.5) / W
            dy = abs((y + 0.5) - cy) / (H / 2.0)
            # streak: hot at the leading (right) end, tapering left
            head = max(0.0, 1.0 - abs(u - 0.78) / 0.30) ** 1.4
            tail = max(0.0, u) ** 2.2
            thick = max(0.06, 0.20 + 0.80 * tail)
            prof = max(0.0, 1.0 - (dy / thick)) ** 1.6
            a = min(1.0, prof * (0.35 * tail + head))
            if a <= 0.004:
                continue
            k = 0.35 + 0.65 * a
            px[x, y] = (int(255 * k), int((200 + 55 * head) * k),
                        int((130 + 110 * head) * k), int(255 * a))
    return img


def make_particle_glow():
    return radial(64, 64, 32, 32, 32, (255, 244, 220), power=2.4, inner=0.06)


def make_particle_smoke(rng):
    W = H = 48
    n = value_noise(W, H, 3, rng, octaves=4)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    px = img.load()
    cx = cy = W / 2.0
    for y in range(H):
        for x in range(W):
            d = math.hypot((x + 0.5) - cx, (y + 0.5) - cy) / (W / 2.0)
            falloff = max(0.0, 1.0 - d) ** 1.9
            v = n[y * W + x]
            a = falloff * (0.30 + 0.95 * v)
            a = min(1.0, max(0.0, (a - 0.05) * 1.35))
            if a <= 0.004:
                continue
            g = 0.55 + 0.45 * v
            k = 0.30 + 0.70 * a
            px[x, y] = (int(210 * g * k), int(198 * g * k), int(196 * g * k),
                        int(255 * a))
    return img.filter(ImageFilter.GaussianBlur(0.8))


def make_particle_ring():
    return ring(64, 64, 32, 32, 24, 8.0, (200, 235, 255), power=1.9)


def make_particle_shard():
    W = H = 12
    s = Sheet(W, H)
    s.poly([(11, 2), (6, 6.5), (1.5, 10), (4, 4.5)],
           fill=(255, 236, 190, 255))
    s.poly([(10, 3), (6, 6.2), (4.6, 5.2)], fill=(255, 255, 250, 255))
    img = s.finish()
    g = add(Image.new("RGBA", (W, H), (0, 0, 0, 0)),
            glow_blob(W, H, 6, 6, 6.5, (140, 110, 60), 2.4))
    return add(img, g)


# --------------------------------------------------------------------------
# Background
# --------------------------------------------------------------------------

def make_star_small():
    return radial(8, 8, 4, 4, 4, (220, 232, 255), power=2.6, inner=0.10)


def make_star_large():
    W = H = 16
    base = radial(W, H, 8, 8, 8, (255, 250, 235), power=2.4, inner=0.12)
    s = Sheet(W, H)
    s.poly([(8, 0), (9.0, 7), (16, 8), (9.0, 9), (8, 16), (7.0, 9), (0, 8), (7.0, 7)],
           fill=(255, 255, 250, 190))
    return add(base, s.finish())


def make_nebula(rng):
    W = H = 128
    n = value_noise(W, H, 2, rng, octaves=5)
    m = value_noise(W, H, 4, rng, octaves=3)
    img = Image.new("RGBA", (W, H), (0, 0, 0, 0))
    px = img.load()
    c = W / 2.0
    for y in range(H):
        for x in range(W):
            d = math.hypot((x + 0.5) - c, (y + 0.5) - c) / c
            falloff = max(0.0, 1.0 - d) ** 2.0
            v = n[y * W + x]
            a = falloff * max(0.0, (v - 0.34)) * 2.0
            a = min(1.0, a)
            if a <= 0.004:
                continue
            t = m[y * W + x]
            r = 60 + 130 * t
            g = 40 + 70 * v
            b = 150 + 90 * (1.0 - t)
            k = 0.35 + 0.65 * a
            px[x, y] = (int(r * k), int(g * k), int(b * k), int(255 * a))
    img = img.filter(ImageFilter.GaussianBlur(1.6))
    # a scatter of embedded stars
    d = ImageDraw.Draw(img, "RGBA")
    for _ in range(26):
        sx, sy = rng.uniform(10, W - 10), rng.uniform(10, H - 10)
        rr = rng.uniform(0.6, 1.5)
        d.ellipse([sx - rr, sy - rr, sx + rr, sy + rr],
                  fill=(230, 236, 255, int(rng.uniform(90, 190))))
    return img


def make_white():
    return Image.new("RGBA", (4, 4), (255, 255, 255, 255))


# --------------------------------------------------------------------------
# Build, pack, emit
# --------------------------------------------------------------------------

def build_sprites():
    rng = random.Random(SEED)
    sprites = {}

    sprites["ship_player"] = make_player(0)
    sprites["ship_player_bank_up"] = make_player(+1)
    sprites["ship_player_bank_down"] = make_player(-1)

    sprites["enemy_drifter"] = make_drifter()
    sprites["enemy_waverider"] = make_waverider()
    sprites["enemy_diver"] = make_diver()
    sprites["enemy_turret"] = make_turret()
    sprites["enemy_splitter"] = make_splitter(random.Random(SEED + 11))
    sprites["enemy_splitter_child"] = make_splitter_child(random.Random(SEED + 12))
    sprites["enemy_orbiter"] = make_orbiter()
    sprites["enemy_mine"] = make_mine()
    sprites["enemy_boss"] = make_boss()

    sprites["bullet_player"] = make_bullet_player()
    sprites["missile_player"] = make_missile(True)
    sprites["lance_player"] = make_lance()
    sprites["laser_segment"] = make_laser_segment()
    sprites["laser_head"] = make_laser_head()

    sprites["bullet_enemy"] = make_bullet_enemy(False)
    sprites["bullet_enemy_heavy"] = make_bullet_enemy(True)
    sprites["missile_enemy"] = make_missile(False)

    sprites.update(make_powerups())

    sprites["particle_spark"] = make_particle_spark()
    sprites["particle_glow"] = make_particle_glow()
    sprites["particle_smoke"] = make_particle_smoke(random.Random(SEED + 21))
    sprites["particle_ring"] = make_particle_ring()
    sprites["particle_shard"] = make_particle_shard()

    sprites["star_small"] = make_star_small()
    sprites["star_large"] = make_star_large()
    sprites["nebula_patch"] = make_nebula(random.Random(SEED + 31))

    sprites["white"] = make_white()

    missing = [n for n in REQUIRED if n not in sprites]
    if missing:
        sys.stderr.write("error: generator is missing sprites: %s\n"
                         % ", ".join(missing))
        raise SystemExit(1)
    extra = [n for n in sprites if n not in REQUIRED]
    if extra:
        sys.stderr.write("error: generator produced sprites not in the "
                         "SpriteId contract: %s\n" % ", ".join(sorted(extra)))
        raise SystemExit(1)
    return sprites


def shelf_pack(sprites, size, pad=PAD):
    """Simple shelf packer: tallest-first rows. Returns {name: (x, y)}."""
    order = sorted(REQUIRED,
                   key=lambda n: (-sprites[n].height, -sprites[n].width, n))
    placed = {}
    x = y = pad
    shelf_h = 0
    for name in order:
        im = sprites[name]
        w, h = im.width, im.height
        if w + 2 * pad > size or h + 2 * pad > size:
            sys.stderr.write(
                "error: sprite '%s' is %dx%d which cannot fit in a %dx%d atlas "
                "(with %dpx padding). Increase --size.\n"
                % (name, w, h, size, size, pad))
            raise SystemExit(1)
        if x + w + pad > size:            # new shelf
            x = pad
            y += shelf_h + pad
            shelf_h = 0
        if y + h + pad > size:
            used = sum(s.width * s.height for s in sprites.values())
            sys.stderr.write(
                "error: atlas overflow while placing '%s'. %d sprites need "
                "~%d px^2 of art but only %dx%d is available. Re-run with a "
                "larger --size (e.g. --size %d).\n"
                % (name, len(sprites), used, size, size, size * 2))
            raise SystemExit(1)
        placed[name] = (x, y)
        x += w + pad
        shelf_h = max(shelf_h, h)
    return placed


def main(argv=None):
    ap = argparse.ArgumentParser(description="Generate the Horizon Unseen placeholder art atlas.")
    ap.add_argument("--out", default="assets", help="output directory (default: assets)")
    ap.add_argument("--size", type=int, default=1024,
                    help="atlas edge length in pixels, power of two (default: 1024)")
    args = ap.parse_args(argv)

    size = args.size
    if size <= 0 or (size & (size - 1)) != 0:
        sys.stderr.write("error: --size must be a positive power of two, got %d\n" % size)
        return 1

    random.seed(SEED)
    print("horizon unseen :: baking placeholder art")
    print("  seed %#x, supersample %dx, padding %dpx" % (SEED, SS, PAD))

    sprites = build_sprites()
    placed = shelf_pack(sprites, size)

    atlas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    used = 0
    for name in REQUIRED:
        im = sprites[name]
        x, y = placed[name]
        atlas.paste(im, (x, y))
        used += im.width * im.height

    os.makedirs(args.out, exist_ok=True)
    png_path = os.path.join(args.out, "atlas.png")
    json_path = os.path.join(args.out, "atlas.json")

    atlas.save(png_path, "PNG", optimize=True)

    doc = {
        "image": "atlas.png",
        "width": size,
        "height": size,
        "sprites": {n: {"x": placed[n][0], "y": placed[n][1],
                        "w": sprites[n].width, "h": sprites[n].height}
                    for n in REQUIRED},
    }
    with open(json_path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(doc, f, indent=2)
        f.write("\n")

    for name in REQUIRED:
        x, y = placed[name]
        im = sprites[name]
        print("  %-22s %4d,%-4d  %3dx%-3d" % (name, x, y, im.width, im.height))

    occ = 100.0 * used / float(size * size)
    print("packed %d sprites into %dx%d - %d px^2 used, occupancy %.2f%%"
          % (len(REQUIRED), size, size, used, occ))
    print("wrote %s" % png_path)
    print("wrote %s" % json_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
