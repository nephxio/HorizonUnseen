#pragma once

// Single place where an archetype turns into a live enemy.
//
// Levels describe *what* to spawn; the factory decides the stats, the sprite
// and which behaviour object gets plugged in. Nothing else in the game
// constructs an EnemyBase directly, so rebalancing an archetype is a one-file
// change.

#include "Core/GameTypes.h"
#include "Core/SpriteId.h"
#include "Game/Entity.h"
#include "Gameplay/Enemies/EnemyBase.h"

#include <memory>

namespace hu {

// Per-archetype baseline. healthScale/speedScale in EnemySpawnParams multiply
// on top of these.
struct EnemyStats {
    float hitPoints = 10.0f;
    float contactDamage = 25.0f;
    float radius = 18.0f;
    Vector2 size{ 40.0f, 40.0f };
    SpriteId sprite = SpriteId::EnemyDrifter;
    int scoreValue = 100;
};

class EnemyFactory {
public:
    // Returns nullptr for EnemyArchetype::Count or an unknown value.
    static std::unique_ptr<EnemyBase> create(EnemyArchetype archetype,
                                             const EnemySpawnParams& params);

    // Splitter offspring. Not an archetype of its own -- it reports as
    // EnemyArchetype::Splitter with the minion flag set, so the drop table can
    // treat it as chaff.
    static std::unique_ptr<EnemyBase> createSplitterChild(const EnemySpawnParams& params);

    static const EnemyStats& stats(EnemyArchetype archetype);
    static const EnemyStats& splitterChildStats();
};

} // namespace hu
