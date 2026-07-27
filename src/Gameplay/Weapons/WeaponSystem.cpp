#include "Gameplay/Weapons/WeaponSystem.h"

#include "Config/GameConfig.h"
#include "Core/Log.h"
#include "Core/Math.h"

namespace hu {

// ---------------------------------------------------------------------------
// Tuning constants (this file's single tunable block).
//
// GameConfig supplies the baselines that already exist there
// (playerFireRate, playerBulletDamage, playerBulletSpeed); the per-level tables
// below are multipliers on top of those so a config tweak still moves every
// weapon together.
// ---------------------------------------------------------------------------
namespace {

constexpr float MuzzleForwardOffset = 30.0f;   // Nose offset from ship centre.

// --- Bullet ---
// cfg.playerFireRate is 1 shot/second, which is the "one tick" baseline; these
// scale it into an actual cannon cadence.
constexpr float BulletRateScale[MaxWeaponLevel]   = { 6.0f, 7.5f, 9.0f, 11.0f, 13.0f };
constexpr float BulletDamageScale[MaxWeaponLevel] = { 1.0f, 1.35f, 1.7f, 2.1f, 2.6f };
constexpr float BulletSpeedScale[MaxWeaponLevel]  = { 1.0f, 1.08f, 1.16f, 1.24f, 1.32f };
constexpr float BulletSizeScale[MaxWeaponLevel]   = { 1.0f, 1.12f, 1.24f, 1.36f, 1.5f };
constexpr Vector2 BulletBaseSize{ 14.0f, 6.0f };
constexpr float BulletBaseRadius = 4.0f;
constexpr float BulletLifetime = 3.0f;

// --- Spread ---
constexpr float SpreadRateScale[MaxWeaponLevel]   = { 4.0f, 4.2f, 4.4f, 4.6f, 5.5f };
constexpr float SpreadDamageScale[MaxWeaponLevel] = { 0.7f, 0.7f, 0.7f, 0.7f, 1.0f };
constexpr float SpreadSpeedScale = 0.85f;
constexpr Vector2 SpreadBaseSize{ 10.0f, 6.0f };
constexpr float SpreadBaseRadius = 4.0f;
constexpr float SpreadLifetime = 2.2f;

// --- Missile ---
constexpr float MissileRateScale[MaxWeaponLevel]   = { 2.0f, 2.6f, 3.2f, 4.0f, 5.0f };
constexpr int   MissilesPerVolley[MaxWeaponLevel]  = { 1, 1, 2, 2, 3 };
constexpr float MissileDamageScale[MaxWeaponLevel] = { 1.6f, 1.7f, 1.8f, 1.9f, 2.0f };
constexpr float MissileLaunchSpeed = 320.0f;
constexpr float MissileMaxSpeed = 780.0f;
constexpr float MissileAcceleration = 620.0f;
constexpr float MissileTurnRate = 4.2f;
constexpr float MissileLifetime = 4.0f;
constexpr float MissileVolleySpreadRadians = 0.28f;
constexpr Vector2 MissileSize{ 18.0f, 8.0f };
constexpr float MissileRadius = 6.0f;

// --- Laser ---
constexpr float LaserBaseDamagePerSecond = 90.0f;
constexpr float LaserDamagePerLevel = 45.0f;
constexpr float LaserBaseHalfThickness = 5.0f;
constexpr float LaserHalfThicknessPerLevel = 3.0f;
constexpr float LaserSegmentLength = 64.0f;    // Quad length used when drawing.
constexpr float LaserPulseRate = 24.0f;

// Spread fan, in degrees, added level by level. Level 1 is the first three
// entries, level 2 the first five, and so on. Level 5 fires the same eight
// directions as level 4 but with the damage/rate bonus above.
constexpr float SpreadFanDegrees[8] = {
    0.0f, 45.0f, -45.0f,      // L1
    90.0f, -90.0f,            // L2
    180.0f,                   // L3
    135.0f, -135.0f           // L4 (and L5)
};
constexpr int SpreadFanCountForLevel[MaxWeaponLevel] = { 3, 5, 6, 8, 8 };

int clampLevelIndex(int level) {
    if (level < 1) return 0;
    if (level > MaxWeaponLevel) return MaxWeaponLevel - 1;
    return level - 1;
}

std::size_t weaponIndex(WeaponType t) {
    const std::size_t i = static_cast<std::size_t>(t);
    return i < WeaponTypeCount ? i : 0;
}

constexpr Color BulletTint{ 0.75f, 0.95f, 1.0f, 1.0f };
constexpr Color SpreadTint{ 1.0f, 0.85f, 0.45f, 1.0f };
constexpr Color MissileTint{ 1.0f, 0.7f, 0.6f, 1.0f };
constexpr Color LaserCoreTint{ 1.0f, 1.0f, 1.0f, 1.0f };
constexpr Color LaserGlowTint{ 0.35f, 0.85f, 1.0f, 0.55f };

} // namespace

// ---------------------------------------------------------------------------

WeaponSystem::WeaponSystem() {
    reset();
}

void WeaponSystem::reset() {
    m_current = WeaponType::Bullet;
    m_levels[weaponIndex(WeaponType::Bullet)] = 1;
    m_levels[weaponIndex(WeaponType::Spread)] = 0;
    m_levels[weaponIndex(WeaponType::Missile)] = 0;
    m_levels[weaponIndex(WeaponType::Laser)] = 0;
    m_firing = false;
    m_cooldown = 0.0f;
    m_laserActive = false;
    m_laserLength = 0.0f;
    m_laserPulse = 0.0f;
    HU_LOG_INFO("Weapons", "Reset: Bullet L1 equipped, all other weapons locked");
}

int WeaponSystem::level(WeaponType t) const {
    return m_levels[weaponIndex(t)];
}

bool WeaponSystem::unlocked(WeaponType t) const {
    return m_levels[weaponIndex(t)] > 0;
}

void WeaponSystem::setFiring(bool held) {
    m_firing = held;
}

Vector2 WeaponSystem::muzzle(Vector2 shipPosition) {
    return Vector2{ shipPosition.x + MuzzleForwardOffset, shipPosition.y };
}

std::vector<float> WeaponSystem::spreadAngles(int level) {
    const int count = SpreadFanCountForLevel[clampLevelIndex(level)];
    std::vector<float> angles;
    angles.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        angles.push_back(degreesToRadians(SpreadFanDegrees[i]));
    }
    return angles;
}

float WeaponSystem::maxLaserHalfThickness() {
    return LaserBaseHalfThickness + LaserHalfThicknessPerLevel * static_cast<float>(MaxWeaponLevel - 1);
}

float WeaponSystem::laserDamagePerSecond() const {
    const int lvl = level(WeaponType::Laser);
    if (lvl <= 0) {
        return 0.0f;
    }
    return LaserBaseDamagePerSecond + LaserDamagePerLevel * static_cast<float>(lvl - 1);
}

float WeaponSystem::laserHalfThickness() const {
    const int lvl = level(WeaponType::Laser);
    if (lvl <= 0) {
        return 0.0f;
    }
    return LaserBaseHalfThickness + LaserHalfThicknessPerLevel * static_cast<float>(lvl - 1);
}

void WeaponSystem::cycleWeapon(int direction) {
    if (direction == 0) {
        return;
    }
    const int step = direction > 0 ? 1 : -1;
    const int count = static_cast<int>(WeaponTypeCount);
    int index = static_cast<int>(weaponIndex(m_current));

    for (int attempt = 0; attempt < count; ++attempt) {
        index = (index + step + count) % count;
        const WeaponType candidate = static_cast<WeaponType>(index);
        if (unlocked(candidate)) {
            if (candidate != m_current) {
                m_current = candidate;
                m_cooldown = 0.0f;
                m_laserActive = false;
                HU_LOG_INFO("Weapons", "Switched to %s (L%d)",
                            weaponName(m_current), level(m_current));
            }
            return;
        }
    }
    HU_LOG_DEBUG("Weapons", "Cycle ignored: no other weapon unlocked");
}

void WeaponSystem::grantWeaponPowerup(PowerupType type) {
    const WeaponType weapon = weaponForPowerup(type);
    if (weapon == WeaponType::Count) {
        HU_LOG_WARN("Weapons", "grantWeaponPowerup called with non-weapon powerup %s",
                    powerupName(type));
        return;
    }

    int& lvl = m_levels[weaponIndex(weapon)];
    if (lvl == 0) {
        lvl = 1;
        HU_LOG_INFO("Weapons", "%s UNLOCKED at L1", weaponName(weapon));
        // Unlocking a weapon equips it: it is what the player just earned.
        m_current = weapon;
        m_cooldown = 0.0f;
    } else if (lvl < MaxWeaponLevel) {
        ++lvl;
        HU_LOG_INFO("Weapons", "%s upgraded to L%d", weaponName(weapon), lvl);
    } else {
        HU_LOG_DEBUG("Weapons", "%s already at max level %d", weaponName(weapon), MaxWeaponLevel);
    }
}

// ---------------------------------------------------------------------------
// Firing
// ---------------------------------------------------------------------------

void WeaponSystem::fireBullet(Vector2 origin, IGameWorld& world) {
    const GameConfig& cfg = GameConfig::getInstance();
    const int i = clampLevelIndex(level(WeaponType::Bullet));

    ProjectileSpawn spawn;
    spawn.position = origin;
    spawn.velocity = Vector2{ cfg.playerBulletSpeed * BulletSpeedScale[i], 0.0f };
    spawn.damage = cfg.playerBulletDamage * BulletDamageScale[i];
    spawn.radius = BulletBaseRadius * BulletSizeScale[i];
    spawn.size = Vector2{ BulletBaseSize.x * BulletSizeScale[i], BulletBaseSize.y * BulletSizeScale[i] };
    spawn.lifetime = BulletLifetime;
    spawn.motion = ProjectileMotion::Straight;
    spawn.sprite = SpriteId::BulletPlayer;
    spawn.tint = BulletTint;
    spawn.additive = true;
    world.spawnPlayerProjectile(spawn);

    EffectRequest flash;
    flash.kind = EffectKind::MuzzleFlash;
    flash.position = origin;
    flash.direction = Vector2{ 1.0f, 0.0f };
    flash.scale = BulletSizeScale[i];
    flash.tint = BulletTint;
    world.spawnEffect(flash);
}

void WeaponSystem::fireSpread(Vector2 origin, IGameWorld& world) {
    const GameConfig& cfg = GameConfig::getInstance();
    const int lvl = level(WeaponType::Spread);
    const int i = clampLevelIndex(lvl);
    const std::vector<float> angles = spreadAngles(lvl);

    for (float angle : angles) {
        ProjectileSpawn spawn;
        spawn.position = origin;
        spawn.velocity = fromAngle(angle, cfg.playerBulletSpeed * SpreadSpeedScale);
        spawn.damage = cfg.playerBulletDamage * SpreadDamageScale[i];
        spawn.radius = SpreadBaseRadius;
        spawn.size = SpreadBaseSize;
        spawn.lifetime = SpreadLifetime;
        spawn.motion = ProjectileMotion::Straight;
        spawn.sprite = SpriteId::BulletPlayer;
        spawn.tint = SpreadTint;
        spawn.additive = true;
        world.spawnPlayerProjectile(spawn);
    }

    EffectRequest flash;
    flash.kind = EffectKind::MuzzleFlash;
    flash.position = origin;
    flash.scale = 1.1f;
    flash.tint = SpreadTint;
    world.spawnEffect(flash);
}

void WeaponSystem::fireMissile(Vector2 origin, IGameWorld& world) {
    const GameConfig& cfg = GameConfig::getInstance();
    const int lvl = level(WeaponType::Missile);
    const int i = clampLevelIndex(lvl);
    const int count = MissilesPerVolley[i];

    TargetInfo initial;
    const bool haveTarget = world.findNearestEnemy(origin, 0.0f, initial);

    for (int n = 0; n < count; ++n) {
        // Fan the volley slightly so simultaneous missiles do not overlap.
        const float offset = (count > 1)
            ? (static_cast<float>(n) / static_cast<float>(count - 1) - 0.5f) * 2.0f * MissileVolleySpreadRadians
            : 0.0f;

        ProjectileSpawn spawn;
        spawn.position = origin;
        spawn.velocity = fromAngle(offset, MissileLaunchSpeed);
        spawn.damage = cfg.playerBulletDamage * MissileDamageScale[i];
        spawn.radius = MissileRadius;
        spawn.size = MissileSize;
        spawn.lifetime = MissileLifetime;
        spawn.motion = ProjectileMotion::Homing;
        spawn.sprite = SpriteId::MissilePlayer;
        spawn.tint = MissileTint;
        spawn.turnRate = MissileTurnRate;
        spawn.acceleration = MissileAcceleration;
        spawn.maxSpeed = MissileMaxSpeed;
        spawn.emitsTrail = true;
        spawn.trailSprite = SpriteId::ParticleSmoke;
        world.spawnPlayerProjectile(spawn);
    }

    HU_LOG_TRACE("Weapons", "Missile volley of %d (target %s)",
                 count, haveTarget ? "acquired" : "none");
}

void WeaponSystem::updateLaser(float deltaTime, Vector2 origin, IGameWorld& world) {
    m_laserActive = m_firing && unlocked(WeaponType::Laser);
    if (!m_laserActive) {
        return;
    }
    m_laserOrigin = origin;
    // The beam is swept from the nose to the right screen edge every frame, so
    // it tracks the ship instead of being a fire-and-forget projectile.
    m_laserLength = world.screenWidth() - origin.x;
    if (m_laserLength < 0.0f) {
        m_laserLength = 0.0f;
    }
    m_laserPulse += deltaTime * LaserPulseRate;
    if (m_laserPulse > TwoPi) {
        m_laserPulse -= TwoPi;
    }
}

void WeaponSystem::update(float deltaTime, Vector2 shipPosition, IGameWorld& world) {
    const GameConfig& cfg = GameConfig::getInstance();
    const Vector2 origin = muzzle(shipPosition);

    // The laser is continuous, and never uses the cooldown path.
    if (m_current == WeaponType::Laser) {
        m_cooldown = 0.0f;
        updateLaser(deltaTime, origin, world);
        return;
    }
    m_laserActive = false;

    if (m_cooldown > 0.0f) {
        m_cooldown -= deltaTime;
    }
    if (!m_firing || m_cooldown > 0.0f) {
        return;
    }

    float shotsPerSecond = 0.0f;
    switch (m_current) {
        case WeaponType::Bullet:
            shotsPerSecond = cfg.playerFireRate * BulletRateScale[clampLevelIndex(level(WeaponType::Bullet))];
            fireBullet(origin, world);
            break;
        case WeaponType::Spread:
            shotsPerSecond = cfg.playerFireRate * SpreadRateScale[clampLevelIndex(level(WeaponType::Spread))];
            fireSpread(origin, world);
            break;
        case WeaponType::Missile:
            shotsPerSecond = cfg.playerFireRate * MissileRateScale[clampLevelIndex(level(WeaponType::Missile))];
            fireMissile(origin, world);
            break;
        default:
            return;
    }

    m_cooldown = (shotsPerSecond > 0.0f) ? (1.0f / shotsPerSecond) : 0.25f;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void WeaponSystem::appendDraw(DrawList& out) const {
    if (!m_laserActive || m_laserLength <= 0.0f) {
        return;
    }

    const float half = laserHalfThickness();
    const float pulse = 0.85f + 0.15f * std::sin(m_laserPulse);

    // Outer glow: one long soft quad.
    Color glow = LaserGlowTint;
    glow.a *= pulse;
    out.add(SpriteId::LaserSegment,
            Vector2{ m_laserOrigin.x + m_laserLength * 0.5f, m_laserOrigin.y },
            Vector2{ m_laserLength, half * 4.0f },
            DrawLayer::Beam, glow, 0.0f, true);

    // Bright core, emitted as a run of quads so the renderer can keep the
    // texture tiling tight rather than stretching one sprite across the screen.
    const int segments = static_cast<int>(m_laserLength / LaserSegmentLength) + 1;
    const float segmentLength = m_laserLength / static_cast<float>(segments);
    Color core = LaserCoreTint;
    core.a = pulse;
    for (int i = 0; i < segments; ++i) {
        const float centre = m_laserOrigin.x + segmentLength * (static_cast<float>(i) + 0.5f);
        out.add(SpriteId::LaserSegment,
                Vector2{ centre, m_laserOrigin.y },
                Vector2{ segmentLength, half * 2.0f },
                DrawLayer::Beam, core, 0.0f, true);
    }

    // Emitter head at the muzzle.
    out.add(SpriteId::LaserHead, m_laserOrigin,
            Vector2{ half * 5.0f, half * 5.0f },
            DrawLayer::Beam, core, 0.0f, true);
}

} // namespace hu
