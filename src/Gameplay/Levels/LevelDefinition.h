#pragma once

// Pure data description of a level.
//
// A level is a list of timed waves and nothing more. There is no code path that
// special-cases a particular level: LevelDirector plays back whatever it is
// given, and LevelRegistry (Levels.h) is the single source of truth for which
// levels exist. Adding a level means adding one LevelDefinition to that
// registry -- no other system changes.

#include "Core/GameTypes.h"
#include "Core/Vector2.h"
#include "Gameplay/Enemies/EnemyBase.h"

#include <cstddef>
#include <string>
#include <vector>

namespace hu {

// ---------------------------------------------------------------------------
// Authoring defaults
// ---------------------------------------------------------------------------

// Enemies enter from just past the right edge.
inline constexpr float LevelSpawnX = 1340.0f;
inline constexpr float LevelDefaultScrollSpeed = 120.0f;

// Where the boss enters from.
inline constexpr float LevelBossSpawnX = 1500.0f;
inline constexpr float LevelBossSpawnY = 360.0f;

// ---------------------------------------------------------------------------
// Wave data
// ---------------------------------------------------------------------------

// One enemy in a wave. `delay` is relative to the wave's own start time, which
// makes it easy to author a trickle ("five drifters, 0.6s apart") without
// recomputing absolute timestamps.
struct WaveSpawn {
    EnemyArchetype archetype = EnemyArchetype::Drifter;
    Vector2 spawnPosition{ LevelSpawnX, 360.0f };
    float delay = 0.0f;

    // Per-archetype tuning, forwarded into EnemySpawnParams.
    float phase = 0.0f;          // Wave phase / orbit start angle / fire offset.
    float speedScale = 1.0f;     // Multiplies nominal movement speed.
    int formationIndex = 0;      // Position within a squadron.
    float amplitude = 0.0f;      // Sine amplitude / orbit radius; 0 = default.
    float healthScale = 1.0f;    // Multiplies nominal hit points.
};

struct Wave {
    float startTime = 0.0f;
    std::vector<WaveSpawn> spawns;
    std::string name;
};

struct LevelDefinition {
    std::string id;                                  // Stable key, e.g. "test_level".
    std::string displayName;
    float duration = 0.0f;                           // Seconds of wave content before the boss.
    float scrollSpeed = LevelDefaultScrollSpeed;
    std::vector<Wave> waves;
    bool hasBoss = true;
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Converts a wave row into the factory's spawn description. `scrollSpeed` comes
// from the owning level so world-anchored enemies ride the background exactly.
EnemySpawnParams toSpawnParams(const WaveSpawn& spawn, float scrollSpeed);

// Absolute time at which a spawn fires (wave start + row delay).
float absoluteSpawnTime(const Wave& wave, const WaveSpawn& spawn);

// Last absolute spawn time in the level, useful for sanity-checking that
// `duration` leaves room for the final wave to play out.
float lastSpawnTime(const LevelDefinition& level);

std::size_t totalSpawnCount(const LevelDefinition& level);

// --- Authoring conveniences (used by Levels.cpp) ---------------------------

WaveSpawn makeSpawn(EnemyArchetype archetype, float y, float delay);

// A squadron of WaveRiders sharing one wave, offset along it by index so they
// snake through in sequence.
void appendFormation(Wave& wave, EnemyArchetype archetype, float baseY, int count,
                     float spacingSeconds, float amplitude = 0.0f, float startDelay = 0.0f);

} // namespace hu
