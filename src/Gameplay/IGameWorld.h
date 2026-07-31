#pragma once

// The seam between gameplay subsystems and the scene that owns them.
//
// Weapons, enemies, power-ups and effects all need to spawn things, query
// things and shake the screen, but none of them should know about GameWorld
// directly. They talk through this interface instead, which keeps each
// subsystem independently testable and stops the scene from becoming a
// dependency magnet.

#include "Core/DrawList.h"   // Color, DrawLayer
#include "Core/GameTypes.h"
#include "Core/SpriteId.h"
#include "Core/Vector2.h"

#include <cstdint>
#include <vector>

namespace hu {

// ---------------------------------------------------------------------------
// Projectiles
// ---------------------------------------------------------------------------

enum class ProjectileMotion : std::uint8_t {
    Straight = 0,  // Constant velocity.
    Homing,        // Steers toward an acquired target.
    Piercing,      // Straight, but is not consumed on hit.
    Orbiting       // Circles an anchor while drifting forward (Helix Beam).
};

struct ProjectileSpawn {
    Vector2 position{ 0.0f, 0.0f };
    Vector2 velocity{ 0.0f, 0.0f };
    float damage = 10.0f;
    float radius = 4.0f;              // Collision radius.
    float lifetime = 5.0f;            // Seconds before self-expiry.
    ProjectileMotion motion = ProjectileMotion::Straight;
    SpriteId sprite = SpriteId::BulletPlayer;
    Vector2 size{ 12.0f, 6.0f };
    Color tint{};
    bool additive = false;

    // Homing tuning. turnRate is radians/second; 0 disables steering even when
    // motion is Homing, which is how a missile "loses lock".
    float turnRate = 4.0f;
    float acceleration = 0.0f;
    float maxSpeed = 900.0f;

    // Piercing projectiles keep going until they have hit this many targets.
    int pierceCount = 1;

    // Trail emission: particle systems read these to decorate the projectile.
    bool emitsTrail = false;
    SpriteId trailSprite = SpriteId::ParticleSpark;
};

// ---------------------------------------------------------------------------
// Effects
// ---------------------------------------------------------------------------

enum class EffectKind : std::uint8_t {
    MuzzleFlash = 0,
    Impact,
    Explosion,
    BigExplosion,
    PowerupPickup,
    CellBreak,
    CellCharge,
    SuperweaponCharge,
    ScreenClear,
    Thruster,
    Debris
};

struct EffectRequest {
    EffectKind kind = EffectKind::Impact;
    Vector2 position{ 0.0f, 0.0f };
    Vector2 direction{ 1.0f, 0.0f };
    float scale = 1.0f;
    Color tint{};
};

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

// A lightweight, non-owning view of a targetable enemy. `handle` is an opaque
// identifier the scene can resolve back to the real enemy; homing projectiles
// store it so they keep tracking the same target across frames.
struct TargetInfo {
    Vector2 position{ 0.0f, 0.0f };
    Vector2 velocity{ 0.0f, 0.0f };
    float radius = 16.0f;
    std::uint32_t handle = 0;
    bool isBoss = false;
};

inline constexpr std::uint32_t InvalidTarget = 0;

class IGameWorld {
public:
    virtual ~IGameWorld() = default;

    // --- Spawning -----------------------------------------------------------
    virtual void spawnPlayerProjectile(const ProjectileSpawn& spawn) = 0;
    virtual void spawnEnemyProjectile(const ProjectileSpawn& spawn) = 0;
    virtual void spawnPowerup(PowerupType type, Vector2 position) = 0;
    virtual void spawnEffect(const EffectRequest& request) = 0;

    // --- Queries ------------------------------------------------------------
    virtual Vector2 playerPosition() const = 0;

    // Nearest enemy to `from` within `maxDistance` (<= 0 means unlimited).
    // Returns false when nothing is targetable.
    virtual bool findNearestEnemy(Vector2 from, float maxDistance, TargetInfo& out) const = 0;

    // Up to `maxCount` enemies nearest to `from`, used to distribute the
    // missile barrage across distinct targets.
    virtual std::vector<TargetInfo> findEnemies(Vector2 from, float maxDistance, int maxCount) const = 0;

    // Re-resolves a handle from a previous query. Returns false once the target
    // is dead or off-screen, which tells a missile to fly straight or re-acquire.
    virtual bool resolveTarget(std::uint32_t handle, TargetInfo& out) const = 0;

    // --- World effects ------------------------------------------------------
    // Energy Bomb: wipes non-boss enemies and heavily damages bosses.
    virtual void clearScreen(float bossDamage) = 0;
    virtual void addScreenShake(float intensity, float duration) = 0;

    virtual DifficultyMode difficulty() const = 0;
    virtual float elapsedLevelTime() const = 0;

    // Screen bounds in world pixels, so systems can cull without hard-coding
    // the resolution.
    virtual float screenWidth() const = 0;
    virtual float screenHeight() const = 0;
};

} // namespace hu
