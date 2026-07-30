# Horizon Unseen

A 2D side-scrolling shoot-'em-up in the vein of R-Type, built with Vulkan and ImGui.

The ship holds the left of the screen, the world scrolls past, and survival depends on
managing five Energy Cells that serve as hit points *and* superweapon fuel at the same
time.

## Building

### Prerequisites

- CMake 3.20+
- Vulkan SDK (with `VULKAN_SDK` set)
- A C++17 compiler (MSVC, GCC, or Clang)
- Python 3 with Pillow, only if you want to regenerate the placeholder art

### Dependencies

GLFW and ImGui are git submodules pinned to specific commits; `stb_image.h` is
vendored directly in `external/stb/`. Clone with submodules:

```bash
git clone --recurse-submodules https://github.com/nephxio/HorizonUnseen.git
```

If you already cloned without them:

```bash
git submodule update --init --recursive
```

They are pinned rather than tracking a branch, so every checkout builds against
the same versions. Treat both as read-only drop-ins: fix problems in `src/`, not
in the vendored trees, or the changes will be lost on the next upgrade.

### Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

On Windows this needs the MSVC environment loaded (`vcvars64.bat`), otherwise the
compiler cannot find the CRT headers.

Both executables and their `shaders/` and `assets/` folders land in the build directory,
so run them from there:

```bash
cd build && ./HorizonUnseen
```

## Controls

| Input | Action |
|---|---|
| `WASD` / Arrow keys | Move |
| `Space` | Fire current weapon |
| `Left Shift` / `Right Shift` | Fire superweapon |
| `[` / `]` | Cycle weapon backward / forward |
| `Esc` | Pause |
| `` ` `` (backtick) | Toggle the debug console |

## Game mechanics

### Energy Cells

Five cells, numbered 1-5. Each has hit points, an energy charge, and a damage-rate
threshold. Damage is routed by the **rolling 5-second damage rate**:

- **Below** the threshold, the ship absorbs the hit: charge is added to the lowest
  unfilled cell, starting at cell 1 and working up.
- **Above** the threshold, the ship is overwhelmed: hit points are stripped from the
  highest unbroken cell, starting at cell 5 and working down.

Cell 1 is the armoured battery (most HP, least charge); cell 5 is the fragile capacitor
(least HP, most charge). A cell at 0 HP *breaks* and can only be restored by a Cell
Repair drop. All five broken means death.

The HUD shows this directly: per-cell HP and charge, plus a damage-pressure bar with the
threshold marked, reading `CHARGING` or `BREAKING CELLS`.

### Weapons

Cycle with `[` and `]`. Only Bullet is available at the start; the rest are unlocked by
their power-up, and each further pick-up raises that weapon's level to a maximum of 5.

| Weapon | Behaviour |
|---|---|
| Bullet | Straight ballistic shot from the nose |
| Spread | Multi-directional fan; higher levels add up/down, backward, and back-diagonals |
| Missile | Homing; higher levels fire faster and more per volley |
| Laser | Continuous beam that sweeps with the ship; higher levels are thicker |

### Superweapons

Fired with either Shift. The tier is chosen by how many cells are fully charged, and
firing consumes that charge.

| Cells | Weapon | Effect |
|---|---|---|
| 1 | Piercing Lance | Heavy armour-piercing slug, gently homing, passes through everything |
| 2 | Missile Barrage | A swarm of homing missiles distributed across on-screen targets |
| 3 | Laser Spread | Beams in every direction the Spread weapon has unlocked |
| 4 | Helix Beam | A beam twice the thickness of a level-5 laser, with missiles spiralling around it |
| 5 | Energy Bomb | Clears all non-boss enemies and heavily damages bosses |

### Enemies

Eight archetypes, each with its own movement and weapon: **Drifter** (fodder),
**Wave Rider** (sine-wave formations), **Diver** (dives at the player and withdraws),
**Turret** (world-anchored, leads its shots), **Splitter** (breaks into children),
**Orbiter** (traces a cycloid, fires tangentially), **Mine** (detonates into a bullet
ring), and a three-phase **Boss**.

### Grazing

A bullet that passes close without hitting you awards energy charge. This is
what keeps the cell economy working under heavy fire: dense patterns would
otherwise pin you permanently over the damage threshold, turning a risk/reward
system into pure attrition. Grazing means more bullets on screen translates into
more superweapon fuel, and rewards flying *into* danger rather than away from it.

In Bullet Hell the ship's hitbox shrinks to a few pixels — far smaller than the
sprite — and is drawn as a bright pulsing core, ringed by the graze band. Dense
patterns are only fair when you can thread a gap the ship appears not to fit
through.

### Secrets and Bullet Hell

Each level defines secrets with conditions such as reaching a hidden location, destroying
specific enemies within a time limit, surviving a window without damage, passing through
a stretch without firing, or collecting power-ups in a set order.

Finding **every secret in every level** unlocks Bullet Hell mode on the main menu, where
every enemy keeps its identity but fires a far denser pattern — orbiter spirals, wave-rider
ribbons, turret lighthouse sweeps, mine double-detonations, splitter cascades, and a boss
counter-spiral. The unlock is derived by walking the level and secret registries, so adding
a level automatically raises the requirement — no other code needs to change.

> **Playtest builds** set `DevAlwaysUnlockBulletHell` in `src/Application.cpp` to open the
> mode from the menu without earning it, so it can be tested. Progression is still tracked
> underneath and the button is labelled accordingly. Set it to `false` for a real release.

## Adding a level

Add one `LevelDefinition` to the marked block in `src/Gameplay/Levels/Levels.cpp`, and
its secrets to `src/Gameplay/Secrets/SecretRegistry.cpp`. The menus, save file, and
Bullet Hell unlock all pick it up automatically.

## Project structure

```
src/
├── Core/          Logging, math, shared types, SpriteId, DrawList  (no Vulkan/GLFW)
├── Gameplay/      Simulation; talks to the scene through IGameWorld
│   ├── Enemies/     Archetypes, behaviours, boss
│   ├── Levels/      Level data, registry, director
│   ├── Particles/   Pooled particles, effect library, starfield
│   ├── Power/       Energy cells, power-ups and drop tables
│   ├── Projectiles/ Pooled projectiles
│   ├── Secrets/     Secret definitions, registry, runtime tracker
│   ├── Weapons/     Weapons and superweapons
│   └── GameWorld    The scene: owns every system, implements IGameWorld
├── Rendering/     Batched instanced sprite renderer, atlas, textures
├── UI/            HUD, menus, debug overlay (renders from plain view models)
├── Editor/        Separate editor application
└── Application    State machine wiring gameplay, UI and rendering together
```

The layering is deliberate: gameplay never includes a graphics header, the UI never
touches a gameplay object (it renders from the view models in `UI/UiModel.h`), and
`Application` is the only place that depends on all three.

## Art

Placeholder art is generated procedurally:

```bash
python tools/generate_art.py --out assets
```

This writes `assets/atlas.png` and `assets/atlas.json`. The sprite names in that JSON are
a hard contract with the `hu::SpriteId` enum in `src/Core/SpriteId.h`; the renderer logs a
warning for any sprite it cannot resolve and falls back to a white quad.

## Debugging

Everything logs through `Core/Log.h` to stdout, to `logs/HorizonUnseen.log`, and to an
in-memory ring buffer. Press `` ` `` in game for a console with:

- a filterable, pausable log view with per-level and per-category filtering
- live counters for entities, projectiles, particles and draw calls
- a table of the energy cells' internal HP/charge state and the exact damage-rate
  decision being made each frame

Log lines are stamped with frame number and time so subsystems can be correlated within a
single frame.

## License

MIT
