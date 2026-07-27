#include "Gameplay/Power/PowerupSystem.h"

#include "Core/Log.h"
#include "Core/Math.h"

namespace hu {

// ---------------------------------------------------------------------------
// Tuning constants and the drop table (this file's single tunable block).
// ---------------------------------------------------------------------------
namespace {

constexpr float PickupRadius = 22.0f;
constexpr float PlayerPickupRadius = 26.0f;
constexpr Vector2 PickupSize{ 28.0f, 28.0f };
constexpr float PickupLifetime = 14.0f;
constexpr float PickupDriftScale = 0.6f;    // Fraction of the scroll speed.
constexpr float PickupBobAmplitude = 14.0f; // Pixels.
constexpr float PickupBobRate = 2.6f;       // Radians/second.
constexpr float PickupCullMargin = 80.0f;
constexpr float PickupFadeSeconds = 3.0f;   // Blink-out window before expiry.
constexpr float PickupBlinkRate = 12.0f;

// Chance that an enemy of each archetype drops anything at all, indexed by
// EnemyArchetype. Tougher enemies drop far more often; the Boss always drops.
constexpr float DropChance[EnemyArchetypeCount] = {
    0.06f,   // Drifter   - fodder
    0.08f,   // WaveRider
    0.10f,   // Diver
    0.14f,   // Turret    - stationary but dangerous
    0.12f,   // Splitter
    0.12f,   // Orbiter
    0.05f,   // Mine      - passive hazard, rarely worth a drop
    1.00f    // Boss      - guaranteed
};

// Bullet hell throws far more at the player, so it is correspondingly more
// generous with drops.
constexpr float BulletHellDropMultiplier = 1.35f;

// Relative weights over PowerupType, in declaration order:
//   { WeaponSpread, WeaponMissile, WeaponLaser, BulletUpgrade, CellRepair, EnergyCharge }
// Weak enemies mostly cough up energy and cannon upgrades; tough enemies are
// weighted toward the rarer weapon modules.
constexpr int DropWeights[EnemyArchetypeCount][PowerupTypeCount] = {
    { 10,  8,  4, 22, 16, 40 },   // Drifter
    { 10,  8,  4, 22, 16, 40 },   // WaveRider
    { 16, 14, 10, 20, 18, 22 },   // Diver
    { 20, 18, 16, 16, 18, 12 },   // Turret
    { 16, 14, 10, 20, 18, 22 },   // Splitter
    { 16, 14, 10, 20, 18, 22 },   // Orbiter
    { 10,  8,  4, 22, 16, 40 },   // Mine
    { 24, 22, 20, 12, 14,  8 }    // Boss
};

constexpr Color PickupTint{ 1.0f, 1.0f, 1.0f, 1.0f };

std::size_t archetypeIndex(EnemyArchetype archetype) {
    const std::size_t i = static_cast<std::size_t>(archetype);
    return i < EnemyArchetypeCount ? i : 0;
}

} // namespace

// ---------------------------------------------------------------------------

void PowerupSystem::reset() {
    m_pickups.clear();
    m_collected.clear();
    HU_LOG_INFO("Powerup", "Power-up system reset");
}

SpriteId PowerupSystem::spriteFor(PowerupType type) {
    switch (type) {
        case PowerupType::WeaponSpread:  return SpriteId::PowerupSpread;
        case PowerupType::WeaponMissile: return SpriteId::PowerupMissile;
        case PowerupType::WeaponLaser:   return SpriteId::PowerupLaser;
        case PowerupType::BulletUpgrade: return SpriteId::PowerupCannon;
        case PowerupType::CellRepair:    return SpriteId::PowerupRepair;
        case PowerupType::EnergyCharge:  return SpriteId::PowerupEnergy;
        default:                         return SpriteId::PowerupEnergy;
    }
}

PowerupType PowerupSystem::rollType(EnemyArchetype archetype) {
    const std::size_t row = archetypeIndex(archetype);

    int total = 0;
    for (std::size_t i = 0; i < PowerupTypeCount; ++i) {
        total += DropWeights[row][i];
    }
    if (total <= 0) {
        return PowerupType::EnergyCharge;
    }

    int roll = static_cast<int>(randomRange(0.0f, static_cast<float>(total)));
    if (roll >= total) {
        roll = total - 1;
    }
    for (std::size_t i = 0; i < PowerupTypeCount; ++i) {
        roll -= DropWeights[row][i];
        if (roll < 0) {
            return static_cast<PowerupType>(i);
        }
    }
    return PowerupType::EnergyCharge;
}

bool PowerupSystem::maybeDropPowerup(Vector2 position, EnemyArchetype archetype, DifficultyMode mode) {
    float chance = DropChance[archetypeIndex(archetype)];
    if (mode == DifficultyMode::BulletHell) {
        chance *= BulletHellDropMultiplier;
    }
    if (chance > 1.0f) {
        chance = 1.0f;
    }

    if (randomUnit() >= chance) {
        return false;
    }

    const PowerupType type = rollType(archetype);
    spawn(type, position);
    HU_LOG_DEBUG("Powerup", "%s dropped %s (chance %.2f)",
                 enemyArchetypeName(archetype), powerupName(type), static_cast<double>(chance));
    return true;
}

void PowerupSystem::spawn(PowerupType type, Vector2 position) {
    Powerup pickup;
    pickup.type = type;
    pickup.position = position;
    pickup.velocity = Vector2{ 0.0f, 0.0f };
    pickup.bobPhase = randomRange(0.0f, TwoPi);
    pickup.age = 0.0f;
    pickup.lifetime = PickupLifetime;
    pickup.alive = true;

    // Reuse a dormant slot when one is available so a long level does not grow
    // the vector without bound.
    for (Powerup& slot : m_pickups) {
        if (!slot.alive) {
            slot = pickup;
            return;
        }
    }
    m_pickups.push_back(pickup);
}

void PowerupSystem::update(float deltaTime, Vector2 playerPosition, float scrollSpeed,
                           IGameWorld& world) {
    m_collected.clear();

    const float minX = -PickupCullMargin;
    const float collectDistanceSq =
        (PickupRadius + PlayerPickupRadius) * (PickupRadius + PlayerPickupRadius);

    for (Powerup& pickup : m_pickups) {
        if (!pickup.alive) {
            continue;
        }

        pickup.age += deltaTime;
        if (pickup.lifetime > 0.0f && pickup.age >= pickup.lifetime) {
            pickup.alive = false;
            HU_LOG_DEBUG("Powerup", "%s expired uncollected", powerupName(pickup.type));
            continue;
        }

        // Drift left with the scroll, a little slower than the background so a
        // pickup stays reachable, plus a vertical bob.
        pickup.bobPhase += PickupBobRate * deltaTime;
        pickup.position.x -= scrollSpeed * PickupDriftScale * deltaTime;
        pickup.position.y += std::cos(pickup.bobPhase) * PickupBobAmplitude * deltaTime;

        if (pickup.position.x < minX) {
            pickup.alive = false;
            continue;
        }

        if (distanceSquared(pickup.position, playerPosition) <= collectDistanceSq) {
            pickup.alive = false;
            m_collected.push_back(pickup.type);
            HU_LOG_INFO("Powerup", "Collected %s", powerupName(pickup.type));

            EffectRequest fx;
            fx.kind = EffectKind::PowerupPickup;
            fx.position = pickup.position;
            fx.scale = 1.0f;
            fx.tint = PickupTint;
            world.spawnEffect(fx);
        }
    }
}

void PowerupSystem::appendDraw(DrawList& out) const {
    for (const Powerup& pickup : m_pickups) {
        if (!pickup.alive) {
            continue;
        }

        Color tint = PickupTint;
        const float remaining = pickup.lifetime - pickup.age;
        if (pickup.lifetime > 0.0f && remaining < PickupFadeSeconds) {
            // Blink while about to expire so the player knows to hurry.
            tint.a = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(remaining * PickupBlinkRate));
        }

        const float wobble = std::sin(pickup.bobPhase) * 0.15f;
        out.add(spriteFor(pickup.type), pickup.position, PickupSize,
                DrawLayer::Powerup, tint, wobble, false);
    }
}

std::size_t PowerupSystem::activeCount() const {
    std::size_t count = 0;
    for (const Powerup& pickup : m_pickups) {
        if (pickup.alive) {
            ++count;
        }
    }
    return count;
}

} // namespace hu
