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

GLFW, ImGui, OpenAL Soft and Catch2 are git submodules pinned to specific
commits; `stb_image.h` is vendored directly in `external/stb/`. Clone with
submodules:

```bash
git clone --recurse-submodules https://github.com/nephxio/HorizonUnseen.git
```

If you already cloned without them:

```bash
git submodule update --init --recursive
```

They are pinned rather than tracking a branch, so every checkout builds against
the same versions. Treat them as read-only drop-ins: fix problems in `src/`, not
in the vendored trees, or the changes will be lost on the next upgrade.

Catch2 is only configured when the test suite is being built. Configure with
`-DHU_BUILD_TESTS=OFF` to skip it entirely.

OpenAL Soft is **LGPL v2** while this project is MIT, so it is built as a
shared library and linked dynamically, never statically. `OpenAL32.dll` ships
alongside the executable and the release package carries the attribution and
source offer the licence requires. Static linking it would extend LGPL
relinking obligations to the whole game, so `LIBTYPE` is pinned to `SHARED` in
`external/CMakeLists.txt` rather than left to a default.

### Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

On Windows this needs the MSVC environment loaded (`vcvars64.bat`), otherwise the
compiler cannot find the CRT headers.

The executable and its `shaders/` and `assets/` folders land in the build directory,
so run it from there:

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
├── Core/          Logging, math, Vector2, shared types, SpriteId, DrawList  (no Vulkan/GLFW)
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
├── Audio/         OpenAL voice pool and music, driven by plain SoundEvents
├── Sim/           Headless C ABI over GameWorld, for the RL harness
├── UI/            HUD, menus, debug overlay (renders from plain view models)
└── Application    State machine wiring gameplay, UI and rendering together

tests/             Catch2 unit tests for gameplay rules
```

The layering is deliberate: gameplay never includes a graphics or audio header, the
UI never touches a gameplay object (it renders from the view models in
`UI/UiModel.h`), and `Application` is the only place that depends on them all.

Audio follows the same shape as rendering. Gameplay emits `SoundEvent`s the way it
emits a `DrawList` — plain data, queued without knowing whether anything is
listening — and `Application` drains them into the audio engine. Only the game
executable links `Audio/`, which is what keeps `husim` and the test binary free of
any OpenAL dependency and therefore runnable on a machine with no sound device.

## Art

Placeholder art is generated procedurally:

```bash
python tools/generate_art.py --out assets
```

This writes `assets/atlas.png` and `assets/atlas.json`. The sprite names in that JSON are
a hard contract with the `hu::SpriteId` enum in `src/Core/SpriteId.h`; the renderer logs a
warning for any sprite it cannot resolve and falls back to a white quad.

## Audio

Placeholder audio is generated the same way — synthesised from oscillators and
shaped noise, seeded so the output is reproducible:

```bash
python tools/generate_audio.py --out assets
```

This writes `assets/sounds/*.wav` and `assets/sounds.json`, a hard contract with
`hu::SoundId` and `hu::MusicId` in `src/Core/SoundId.h`. Unlike the atlas, a
mismatch here is checked in CI by `tools/check_audio.py`, because a missing sound
is indistinguishable from a volume slider being down.

Effects are mono and panned by where they happened on screen; OpenAL only
spatialises mono sources, so a stereo effect would silently ignore its pan. Music
is stereo, generated at a lower sample rate, and looped seamlessly.

Only the standard library is needed — no Pillow, no numpy.

Volume is controlled from the Options screen and persisted in the save file. If no
audio device is available the game logs a warning and runs silently rather than
failing to start.

## Tests

Two layers, both headless — neither needs a GPU or a display.

**Unit tests** (`tests/`, Catch2) check individual gameplay rules in isolation.
They are built by default and run through ctest:

```bash
ctest --test-dir build --output-on-failure
```

The binary can also be run directly for Catch2's filtering and reporting
options — `./build/HorizonUnseenTests "[cells]"` runs one tag,
`--list-tests` shows what is available.

Two areas are covered so far, both chosen because a bug in them is silent:

- **Energy cells** (`[cells]`) — the absorb-vs-break routing is the most
  load-bearing rule in the game, and a bug there reads as "the game feels
  wrong" rather than as a crash. The tests pin the `GameConfig` values they
  reason about, so rebalancing cannot silently invalidate them.
- **Secrets** (`[secrets]`) — the tracker's evaluation of each condition kind,
  plus integrity checks over the shipped definitions. Since finding every
  secret unlocks Bullet Hell and the total is derived by walking the
  registries, a malformed secret does not error; it just becomes unearnable.
  `SecretRegistryTests.cpp` checks for exactly the shapes that
  `SecretTracker` skips silently.
- **Audio** (`[audio]`) — the wav reader, including truncated and malformed
  input, since chunk sizes come out of the file and are used to index into it;
  the gameplay sound queue and its panning; and volume clamping and
  persistence. Nothing here needs a sound device.

The tracker tests build their own definitions and pass them to
`SecretTracker::onLevelStart`, so they describe the rules rather than whatever
the shipped levels happen to declare.

**Gameplay tests** (`tools/rl/ci_gameplay_test.py`) drive the real simulation
through `src/Sim` and play whole episodes, asserting that waves spawn,
collisions resolve, cells break and episodes terminate:

```bash
python tools/rl/ci_gameplay_test.py
```

It needs `numpy` and `HUSIM_PATH` pointing at the built `husim.dll`, with
`PYTHONPATH` set to `tools/rl`. Both layers run in CI on every push.

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
