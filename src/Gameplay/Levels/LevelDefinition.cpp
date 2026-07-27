#include "Gameplay/Levels/LevelDefinition.h"

#include "Core/Math.h"

namespace hu {

// ===========================================================================
// TUNABLES
// ===========================================================================
// Formation members carry only their index here; WaveRiderBehavior turns that
// index into a phase offset along the shared sine wave, so the rule for how a
// squadron snakes lives in exactly one place.

EnemySpawnParams toSpawnParams(const WaveSpawn& spawn, float scrollSpeed) {
    EnemySpawnParams params;
    params.position = spawn.spawnPosition;
    params.phase = spawn.phase;
    params.speedScale = spawn.speedScale > 0.0f ? spawn.speedScale : 1.0f;
    params.formationIndex = spawn.formationIndex;
    params.amplitude = spawn.amplitude;
    params.healthScale = spawn.healthScale > 0.0f ? spawn.healthScale : 1.0f;
    params.worldScrollSpeed = scrollSpeed > 0.0f ? scrollSpeed : LevelDefaultScrollSpeed;
    return params;
}

float absoluteSpawnTime(const Wave& wave, const WaveSpawn& spawn) {
    return wave.startTime + spawn.delay;
}

float lastSpawnTime(const LevelDefinition& level) {
    float latest = 0.0f;
    for (const Wave& wave : level.waves) {
        for (const WaveSpawn& spawn : wave.spawns) {
            const float t = absoluteSpawnTime(wave, spawn);
            if (t > latest) {
                latest = t;
            }
        }
    }
    return latest;
}

std::size_t totalSpawnCount(const LevelDefinition& level) {
    std::size_t total = 0;
    for (const Wave& wave : level.waves) {
        total += wave.spawns.size();
    }
    return total;
}

WaveSpawn makeSpawn(EnemyArchetype archetype, float y, float delay) {
    WaveSpawn spawn;
    spawn.archetype = archetype;
    spawn.spawnPosition = { LevelSpawnX, y };
    spawn.delay = delay;
    return spawn;
}

void appendFormation(Wave& wave, EnemyArchetype archetype, float baseY, int count,
                     float spacingSeconds, float amplitude, float startDelay) {
    for (int i = 0; i < count; ++i) {
        WaveSpawn spawn = makeSpawn(archetype, baseY,
                                    startDelay + spacingSeconds * static_cast<float>(i));
        spawn.formationIndex = i;
        spawn.amplitude = amplitude;
        wave.spawns.push_back(spawn);
    }
}

} // namespace hu
