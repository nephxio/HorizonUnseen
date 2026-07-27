#include "Gameplay/Enemies/EnemyFactory.h"

#include "Core/Log.h"
#include "Core/Math.h"
#include "Gameplay/Enemies/Boss.h"
#include "Gameplay/Enemies/EnemyBehaviors.h"

#include <utility>

namespace hu {

// ===========================================================================
// TUNABLES -- per-archetype baseline stats.
// ===========================================================================
namespace {

constexpr const char* kLogCategory = "Enemy";

const EnemyStats kDrifterStats   { 12.0f,  25.0f, 16.0f, { 36.0f, 30.0f }, SpriteId::EnemyDrifter,   100 };
const EnemyStats kWaveRiderStats { 16.0f,  25.0f, 15.0f, { 34.0f, 28.0f }, SpriteId::EnemyWaveRider, 150 };
const EnemyStats kDiverStats     { 22.0f,  35.0f, 17.0f, { 40.0f, 26.0f }, SpriteId::EnemyDiver,     250 };
const EnemyStats kTurretStats    { 90.0f,  40.0f, 22.0f, { 46.0f, 46.0f }, SpriteId::EnemyTurret,    400 };
const EnemyStats kSplitterStats  { 60.0f,  35.0f, 30.0f, { 64.0f, 64.0f }, SpriteId::EnemySplitter,  350 };
const EnemyStats kOrbiterStats   { 26.0f,  30.0f, 16.0f, { 34.0f, 34.0f }, SpriteId::EnemyOrbiter,   200 };
const EnemyStats kMineStats      { 8.0f,   45.0f, 18.0f, { 34.0f, 34.0f }, SpriteId::EnemyMine,      120 };
const EnemyStats kBossStats      { 4200.0f, 60.0f, 74.0f, { 190.0f, 150.0f }, SpriteId::EnemyBoss, 10000 };

const EnemyStats kSplitterChildStats{ 8.0f, 30.0f, 12.0f, { 24.0f, 24.0f },
                                      SpriteId::EnemySplitterChild, 60 };

// Applies the shared stat block plus the spawn-time scaling.
void applyStats(EnemyBase& enemy, const EnemyStats& stats, const EnemySpawnParams& params) {
    const float healthScale = params.healthScale > 0.0f ? params.healthScale : 1.0f;
    enemy.setMaxHitPoints(stats.hitPoints * healthScale);
    enemy.setContactDamage(stats.contactDamage);
    enemy.setRadius(stats.radius);
    enemy.setSize(stats.size);
    enemy.setSprite(stats.sprite);
    enemy.setScoreValue(stats.scoreValue);
}

std::unique_ptr<IEnemyBehavior> makeBehavior(EnemyArchetype archetype) {
    switch (archetype) {
        case EnemyArchetype::Drifter:   return std::unique_ptr<IEnemyBehavior>(new DrifterBehavior());
        case EnemyArchetype::WaveRider: return std::unique_ptr<IEnemyBehavior>(new WaveRiderBehavior());
        case EnemyArchetype::Diver:     return std::unique_ptr<IEnemyBehavior>(new DiverBehavior());
        case EnemyArchetype::Turret:    return std::unique_ptr<IEnemyBehavior>(new TurretBehavior());
        case EnemyArchetype::Splitter:  return std::unique_ptr<IEnemyBehavior>(new SplitterBehavior());
        case EnemyArchetype::Orbiter:   return std::unique_ptr<IEnemyBehavior>(new OrbiterBehavior());
        case EnemyArchetype::Mine:      return std::unique_ptr<IEnemyBehavior>(new MineBehavior());
        default:                        return nullptr;
    }
}

} // namespace

const EnemyStats& EnemyFactory::stats(EnemyArchetype archetype) {
    switch (archetype) {
        case EnemyArchetype::Drifter:   return kDrifterStats;
        case EnemyArchetype::WaveRider: return kWaveRiderStats;
        case EnemyArchetype::Diver:     return kDiverStats;
        case EnemyArchetype::Turret:    return kTurretStats;
        case EnemyArchetype::Splitter:  return kSplitterStats;
        case EnemyArchetype::Orbiter:   return kOrbiterStats;
        case EnemyArchetype::Mine:      return kMineStats;
        case EnemyArchetype::Boss:      return kBossStats;
        default:                        return kDrifterStats;
    }
}

const EnemyStats& EnemyFactory::splitterChildStats() {
    return kSplitterChildStats;
}

std::unique_ptr<EnemyBase> EnemyFactory::create(EnemyArchetype archetype,
                                                const EnemySpawnParams& params) {
    if (archetype == EnemyArchetype::Boss) {
        std::unique_ptr<Boss> boss(new Boss(params));
        applyStats(*boss, kBossStats, params);
        // Boss HP is authored, not scaled by the wave row; re-apply the base so
        // a stray healthScale in level data cannot trivialise the fight.
        boss->setMaxHitPoints(kBossStats.hitPoints *
                              (params.healthScale > 0.0f ? params.healthScale : 1.0f));
        HU_LOG_INFO("Boss", "Boss spawned at (%.0f, %.0f) with %.0f HP", params.position.x,
                    params.position.y, boss->maxHitPoints());
        return std::unique_ptr<EnemyBase>(boss.release());
    }

    std::unique_ptr<IEnemyBehavior> behavior = makeBehavior(archetype);
    if (!behavior) {
        HU_LOG_WARN(kLogCategory, "EnemyFactory: no behaviour for archetype %d",
                    static_cast<int>(archetype));
        return nullptr;
    }

    std::unique_ptr<EnemyBase> enemy(new EnemyBase(archetype, params, std::move(behavior)));
    applyStats(*enemy, stats(archetype), params);

    HU_LOG_DEBUG(kLogCategory, "Spawn %s at (%.0f, %.0f)", enemyArchetypeName(archetype),
                 params.position.x, params.position.y);
    return enemy;
}

std::unique_ptr<EnemyBase> EnemyFactory::createSplitterChild(const EnemySpawnParams& params) {
    std::unique_ptr<IEnemyBehavior> behavior(new SplitterChildBehavior());
    std::unique_ptr<EnemyBase> child(
        new EnemyBase(EnemyArchetype::Splitter, params, std::move(behavior)));
    applyStats(*child, kSplitterChildStats, params);
    child->setMinion(true);
    return child;
}

} // namespace hu
