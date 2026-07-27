#include "Gameplay/Levels/Levels.h"

#include "Core/Log.h"
#include "Core/Math.h"

namespace hu {

// ===========================================================================
// TUNABLES -- authoring grid for the level data below.
// ===========================================================================
namespace {

constexpr const char* kLogCategory = "Level";

// Vertical lanes, expressed against a 720-tall play field. Enemies are authored
// on these lanes so waves read consistently.
constexpr float kLaneTop = 120.0f;
constexpr float kLaneHigh = 220.0f;
constexpr float kLaneMidHigh = 290.0f;
constexpr float kLaneMid = 360.0f;
constexpr float kLaneMidLow = 430.0f;
constexpr float kLaneLow = 500.0f;
constexpr float kLaneBottom = 600.0f;

// Turrets read as bolted to the terrain, so they are authored hard against the
// top and bottom edges.
constexpr float kTurretTop = 96.0f;
constexpr float kTurretBottom = 624.0f;

// Mines are scattered rather than lane-aligned; this staggers them in x so the
// field has depth instead of arriving as a column.
constexpr float kMineDepthStep = 78.0f;

// The test level's pre-boss running time. The last spawn lands well before this
// so the screen is clear when the boss arrives.
constexpr float kTestLevelDuration = 158.0f;
constexpr float kTestLevelScrollSpeed = 130.0f;

// ---------------------------------------------------------------------------
// Small authoring helpers
// ---------------------------------------------------------------------------

Wave makeWave(float startTime, const char* name) {
    Wave wave;
    wave.startTime = startTime;
    wave.name = name;
    return wave;
}

void push(Wave& wave, EnemyArchetype archetype, float y, float delay) {
    wave.spawns.push_back(makeSpawn(archetype, y, delay));
}

void pushTuned(Wave& wave, EnemyArchetype archetype, float y, float delay, float speedScale,
               float phase, int formationIndex = 0, float amplitude = 0.0f) {
    WaveSpawn spawn = makeSpawn(archetype, y, delay);
    spawn.speedScale = speedScale;
    spawn.phase = phase;
    spawn.formationIndex = formationIndex;
    spawn.amplitude = amplitude;
    wave.spawns.push_back(spawn);
}

void pushMine(Wave& wave, float y, float delay, int depthIndex) {
    WaveSpawn spawn = makeSpawn(EnemyArchetype::Mine, y, delay);
    spawn.spawnPosition.x = LevelSpawnX + kMineDepthStep * static_cast<float>(depthIndex);
    spawn.phase = randomRange(0.0f, TwoPi);
    wave.spawns.push_back(spawn);
}

// ===========================================================================
// THE TEST LEVEL
//
// Pacing contract: every archetype is introduced alone, given room to be read,
// then reintroduced in combination. Nothing new appears after the 137s mark --
// the finale only recombines what the player has already learned.
// ===========================================================================

LevelDefinition buildTestLevel() {
    LevelDefinition level;
    level.id = "test_level";
    level.displayName = "Derelict Approach";
    level.duration = kTestLevelDuration;
    level.scrollSpeed = kTestLevelScrollSpeed;
    level.hasBoss = true;

    // --- Act I: fodder, taught one idea at a time -------------------------

    // 0:03 -- three drifters, well spaced. Teaches: things come from the right.
    {
        Wave w = makeWave(3.0f, "Contact");
        push(w, EnemyArchetype::Drifter, kLaneMidHigh, 0.0f);
        push(w, EnemyArchetype::Drifter, kLaneMid, 0.9f);
        push(w, EnemyArchetype::Drifter, kLaneMidLow, 1.8f);
        level.waves.push_back(w);
    }

    // 0:11 -- a wider trickle across the full height. Teaches: cover the lanes.
    {
        Wave w = makeWave(11.0f, "Scattered Patrol");
        push(w, EnemyArchetype::Drifter, kLaneTop, 0.0f);
        push(w, EnemyArchetype::Drifter, kLaneLow, 0.6f);
        push(w, EnemyArchetype::Drifter, kLaneHigh, 1.2f);
        push(w, EnemyArchetype::Drifter, kLaneBottom, 1.8f);
        push(w, EnemyArchetype::Drifter, kLaneMid, 2.4f);
        level.waves.push_back(w);
    }

    // --- Act II: WaveRiders -----------------------------------------------

    // 0:21 -- one squadron on its own, so the snaking path is legible.
    {
        Wave w = makeWave(21.0f, "First Serpent");
        appendFormation(w, EnemyArchetype::WaveRider, kLaneMidHigh, 5, 0.35f);
        level.waves.push_back(w);
    }

    // 0:30 -- two squadrons crossing high and low.
    {
        Wave w = makeWave(30.0f, "Twin Serpents");
        appendFormation(w, EnemyArchetype::WaveRider, kLaneHigh, 5, 0.32f, 84.0f);
        appendFormation(w, EnemyArchetype::WaveRider, kLaneLow, 5, 0.32f, 84.0f, 0.55f);
        level.waves.push_back(w);
    }

    // --- Act III: Divers ---------------------------------------------------

    // 0:40 -- two divers alone. The wind-up glow is the lesson here.
    {
        Wave w = makeWave(40.0f, "Interceptors");
        push(w, EnemyArchetype::Diver, kLaneHigh, 0.0f);
        push(w, EnemyArchetype::Diver, kLaneLow, 1.6f);
        level.waves.push_back(w);
    }

    // 0:49 -- divers harassing while drifters occupy the lanes.
    {
        Wave w = makeWave(49.0f, "Hunting Party");
        push(w, EnemyArchetype::Diver, kLaneTop, 0.0f);
        push(w, EnemyArchetype::Diver, kLaneMid, 1.0f);
        push(w, EnemyArchetype::Diver, kLaneBottom, 2.0f);
        push(w, EnemyArchetype::Drifter, kLaneMidHigh, 0.5f);
        push(w, EnemyArchetype::Drifter, kLaneMidLow, 1.5f);
        level.waves.push_back(w);
    }

    // --- Act IV: Turrets ---------------------------------------------------

    // 1:00 -- the gauntlet: alternating top/bottom emplacements. Teaches lead.
    {
        Wave w = makeWave(60.0f, "The Gauntlet");
        pushTuned(w, EnemyArchetype::Turret, kTurretTop, 0.0f, 1.0f, 0.0f);
        pushTuned(w, EnemyArchetype::Turret, kTurretBottom, 1.7f, 1.0f, 0.4f);
        pushTuned(w, EnemyArchetype::Turret, kTurretTop, 3.6f, 1.0f, 0.8f);
        pushTuned(w, EnemyArchetype::Turret, kTurretBottom, 5.3f, 1.0f, 1.2f);
        level.waves.push_back(w);
    }

    // 1:11 -- turret lead plus a squadron forcing movement through it.
    {
        Wave w = makeWave(71.0f, "Crossfire");
        pushTuned(w, EnemyArchetype::Turret, kTurretTop, 0.0f, 1.0f, 0.0f);
        pushTuned(w, EnemyArchetype::Turret, kTurretBottom, 0.9f, 1.0f, 0.6f);
        appendFormation(w, EnemyArchetype::WaveRider, kLaneMid, 4, 0.3f, 110.0f, 2.2f);
        level.waves.push_back(w);
    }

    // --- Act V: Splitters --------------------------------------------------

    // 1:24 -- two blobs. Teaches: killing it is not the end of it.
    {
        Wave w = makeWave(84.0f, "Heavy Elements");
        push(w, EnemyArchetype::Splitter, kLaneMidHigh, 0.0f);
        push(w, EnemyArchetype::Splitter, kLaneMidLow, 1.8f);
        level.waves.push_back(w);
    }

    // 1:34 -- three blobs with drifter chaff; the shards fill the gaps.
    {
        Wave w = makeWave(94.0f, "Division");
        push(w, EnemyArchetype::Splitter, kLaneHigh, 0.0f);
        push(w, EnemyArchetype::Splitter, kLaneMid, 1.2f);
        push(w, EnemyArchetype::Splitter, kLaneLow, 2.4f);
        push(w, EnemyArchetype::Drifter, kLaneTop, 0.8f);
        push(w, EnemyArchetype::Drifter, kLaneBottom, 2.0f);
        level.waves.push_back(w);
    }

    // --- Act VI: Orbiters --------------------------------------------------

    // 1:45 -- three wheels, counter-rotating in pairs (formationIndex parity).
    {
        Wave w = makeWave(105.0f, "Carousel");
        pushTuned(w, EnemyArchetype::Orbiter, kLaneHigh, 0.0f, 1.0f, 0.0f, 0, 84.0f);
        pushTuned(w, EnemyArchetype::Orbiter, kLaneMid, 0.9f, 1.0f, Pi * 0.66f, 1, 96.0f);
        pushTuned(w, EnemyArchetype::Orbiter, kLaneLow, 1.8f, 1.0f, Pi * 1.33f, 2, 84.0f);
        level.waves.push_back(w);
    }

    // 1:55 -- orbiters plus divers: rotating fire and a committed charge.
    {
        Wave w = makeWave(115.0f, "Wheels and Fangs");
        pushTuned(w, EnemyArchetype::Orbiter, kLaneMidHigh, 0.0f, 1.0f, 0.0f, 0, 100.0f);
        pushTuned(w, EnemyArchetype::Orbiter, kLaneMidLow, 0.7f, 1.0f, Pi, 1, 100.0f);
        pushTuned(w, EnemyArchetype::Orbiter, kLaneMid, 1.4f, 1.1f, Pi * 0.5f, 2, 70.0f);
        push(w, EnemyArchetype::Diver, kLaneTop, 2.2f);
        push(w, EnemyArchetype::Diver, kLaneBottom, 3.4f);
        level.waves.push_back(w);
    }

    // --- Act VII: Minefield ------------------------------------------------

    // 2:06 -- static hazards only. Teaches: shoot it early or route around it.
    {
        Wave w = makeWave(126.0f, "Minefield");
        pushMine(w, kLaneTop, 0.0f, 0);
        pushMine(w, kLaneMidHigh, 0.3f, 2);
        pushMine(w, kLaneBottom, 0.6f, 1);
        pushMine(w, kLaneMid, 1.1f, 3);
        pushMine(w, kLaneHigh, 1.5f, 0);
        pushMine(w, kLaneLow, 1.9f, 2);
        pushMine(w, kLaneMidLow, 2.4f, 4);
        pushMine(w, kLaneTop, 2.9f, 3);
        pushMine(w, kLaneBottom, 3.3f, 5);
        level.waves.push_back(w);
    }

    // --- Finale: everything the player has been taught ---------------------

    // 2:17 -- no new archetypes, just all of them at once.
    {
        Wave w = makeWave(137.0f, "Everything At Once");
        pushTuned(w, EnemyArchetype::Turret, kTurretTop, 0.0f, 1.0f, 0.0f);
        pushTuned(w, EnemyArchetype::Turret, kTurretBottom, 0.5f, 1.0f, 0.7f);
        appendFormation(w, EnemyArchetype::WaveRider, kLaneMid, 5, 0.28f, 120.0f, 1.0f);
        push(w, EnemyArchetype::Splitter, kLaneMidHigh, 2.6f);
        pushTuned(w, EnemyArchetype::Orbiter, kLaneLow, 3.2f, 1.0f, 0.0f, 0, 92.0f);
        pushTuned(w, EnemyArchetype::Orbiter, kLaneHigh, 3.8f, 1.0f, Pi, 1, 92.0f);
        push(w, EnemyArchetype::Diver, kLaneMidLow, 4.4f);
        push(w, EnemyArchetype::Diver, kLaneTop, 5.2f);
        pushMine(w, kLaneMid, 5.8f, 1);
        pushMine(w, kLaneBottom, 6.2f, 3);
        pushMine(w, kLaneHigh, 6.6f, 2);
        level.waves.push_back(w);
    }

    // 2:38 -- boss. LevelDirector spawns it once `duration` elapses.
    return level;
}

// ---------------------------------------------------------------------------
// Registry storage
// ---------------------------------------------------------------------------

const std::vector<LevelDefinition>& registry() {
    // Function-local static: built once, on first use, and never mutated after,
    // so every reference handed out stays valid.
    static const std::vector<LevelDefinition> levels = [] {
        std::vector<LevelDefinition> out;
        out.push_back(buildTestLevel());

        // ADD NEW LEVELS HERE. Nothing else in the game needs to change: the
        // level select, save data and the secrets system all enumerate this
        // registry.

        for (const LevelDefinition& level : out) {
            HU_LOG_INFO(kLogCategory,
                        "Registered level '%s' (%s): %zu waves, %zu spawns, %.0fs + %s",
                        level.id.c_str(), level.displayName.c_str(), level.waves.size(),
                        totalSpawnCount(level), level.duration,
                        level.hasBoss ? "boss" : "no boss");
        }
        return out;
    }();
    return levels;
}

} // namespace

const std::vector<LevelDefinition>& LevelRegistry::all() {
    return registry();
}

const LevelDefinition* LevelRegistry::find(const std::string& id) {
    const std::vector<LevelDefinition>& levels = registry();
    for (const LevelDefinition& level : levels) {
        if (level.id == id) {
            return &level;
        }
    }
    return nullptr;
}

std::size_t LevelRegistry::count() {
    return registry().size();
}

const LevelDefinition* LevelRegistry::at(std::size_t index) {
    const std::vector<LevelDefinition>& levels = registry();
    if (index >= levels.size()) {
        return nullptr;
    }
    return &levels[index];
}

std::size_t LevelRegistry::indexOf(const std::string& id) {
    const std::vector<LevelDefinition>& levels = registry();
    for (std::size_t i = 0; i < levels.size(); ++i) {
        if (levels[i].id == id) {
            return i;
        }
    }
    return levels.size();
}

} // namespace hu
