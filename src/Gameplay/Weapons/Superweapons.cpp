#include "Gameplay/Weapons/Superweapons.h"

#include "Config/GameConfig.h"
#include "Core/Log.h"
#include "Core/Math.h"
#include "Gameplay/Power/EnergyCellSystem.h"
#include "Gameplay/Weapons/WeaponSystem.h"

namespace hu {

// ---------------------------------------------------------------------------
// Tuning constants (this file's single tunable block).
// ---------------------------------------------------------------------------
namespace {

constexpr float MuzzleForwardOffset = 30.0f;

// --- Piercing Lance (1 cell) ---
constexpr float LanceSpeed = 1500.0f;
constexpr float LanceDamage = 420.0f;
constexpr float LanceRadius = 14.0f;
constexpr float LanceLifetime = 3.0f;
constexpr float LanceTurnRate = 0.7f;      // Gentle correction, not a seeker.
constexpr int   LancePierceCount = 999;    // Effectively "never consumed".
constexpr Vector2 LanceSize{ 92.0f, 22.0f };

// --- Missile Barrage (2 cells) ---
constexpr int   BarrageMissileCount = 18;
constexpr float BarrageFanRadians = 2.4f;     // Total fan width at launch.
constexpr float BarrageLaunchSpeed = 260.0f;
constexpr float BarrageMaxSpeed = 900.0f;
constexpr float BarrageAcceleration = 800.0f;
constexpr float BarrageTurnRate = 6.0f;
constexpr float BarrageDamage = 70.0f;
constexpr float BarrageRadius = 7.0f;
constexpr float BarrageLifetime = 5.0f;
constexpr Vector2 BarrageSize{ 20.0f, 9.0f };
constexpr float BarrageTargetSearchRadius = 0.0f;   // 0 == unlimited.

// --- Laser Spread (3 cells) ---
constexpr float LaserSpreadDuration = 1.8f;
constexpr float LaserSpreadDamagePerSecond = 380.0f;
constexpr float LaserSpreadHalfThickness = 16.0f;

// --- Helix Beam (4 cells) ---
constexpr float HelixDuration = 2.4f;
constexpr float HelixDamagePerSecond = 620.0f;
constexpr float HelixEscortInterval = 0.10f;
constexpr float HelixEscortSpeed = 520.0f;
constexpr float HelixEscortDamage = 60.0f;
constexpr float HelixEscortRadius = 7.0f;
constexpr float HelixEscortLifetime = 2.6f;
constexpr Vector2 HelixEscortSize{ 18.0f, 8.0f };

// --- Energy Bomb (5 cells) ---
constexpr float BombBossDamage = 2500.0f;
constexpr float BombShakeIntensity = 34.0f;
constexpr float BombShakeDuration = 1.2f;

// Shared beam presentation.
constexpr float BeamSegmentQuadLength = 96.0f;
constexpr float BeamPulseRate = 30.0f;
constexpr float BeamShakeIntensity = 8.0f;

constexpr Color LanceTint{ 1.0f, 0.95f, 0.6f, 1.0f };
constexpr Color BarrageTint{ 1.0f, 0.6f, 0.5f, 1.0f };
constexpr Color BeamCoreTint{ 1.0f, 1.0f, 1.0f, 1.0f };
constexpr Color BeamGlowTint{ 0.6f, 0.55f, 1.0f, 0.5f };
constexpr Color BombTint{ 0.75f, 0.9f, 1.0f, 1.0f };

// A beam always reaches past the far corner of the screen.
float beamReach(const IGameWorld& world) {
    const float w = world.screenWidth();
    const float h = world.screenHeight();
    return std::sqrt(w * w + h * h) * 1.2f;
}

Vector2 muzzleOf(Vector2 shipPosition) {
    return Vector2{ shipPosition.x + MuzzleForwardOffset, shipPosition.y };
}

} // namespace

// ---------------------------------------------------------------------------

void SuperweaponSystem::reset() {
    m_active = SuperweaponType::None;
    m_timer = 0.0f;
    m_duration = 0.0f;
    m_fireRequested = false;
    m_beams.clear();
    m_beamAngles.clear();
    m_beamHalfThickness = 0.0f;
    m_beamDamagePerSecond = 0.0f;
    m_helixSpawnTimer = 0.0f;
    m_helixEscortIndex = 0;
    m_pulse = 0.0f;
    HU_LOG_INFO("Super", "Superweapon system reset");
}

void SuperweaponSystem::requestFire() {
    m_fireRequested = true;
}

SuperweaponType SuperweaponSystem::pending(const EnergyCellSystem& cells) const {
    if (isActive()) {
        return SuperweaponType::None;   // One beam at a time.
    }
    return superweaponForCharge(cells.chargedCellCount());
}

// ---------------------------------------------------------------------------
// Individual tiers
// ---------------------------------------------------------------------------

void SuperweaponSystem::firePiercingLance(Vector2 origin, IGameWorld& world) {
    // Homing motion with a deliberately tiny turn rate: the slug corrects a few
    // degrees toward the nearest enemy but essentially flies straight. The huge
    // pierce budget is what makes it pass through everything it hits.
    ProjectileSpawn spawn;
    spawn.position = origin;
    spawn.velocity = Vector2{ LanceSpeed, 0.0f };
    spawn.damage = LanceDamage;
    spawn.radius = LanceRadius;
    spawn.size = LanceSize;
    spawn.lifetime = LanceLifetime;
    spawn.motion = ProjectileMotion::Homing;
    spawn.sprite = SpriteId::LancePlayer;
    spawn.tint = LanceTint;
    spawn.additive = true;
    spawn.turnRate = LanceTurnRate;
    spawn.maxSpeed = LanceSpeed;
    spawn.pierceCount = LancePierceCount;
    spawn.emitsTrail = true;
    spawn.trailSprite = SpriteId::ParticleGlow;
    world.spawnPlayerProjectile(spawn);

    EffectRequest fx;
    fx.kind = EffectKind::SuperweaponCharge;
    fx.position = origin;
    fx.scale = 2.0f;
    fx.tint = LanceTint;
    world.spawnEffect(fx);
    world.addScreenShake(10.0f, 0.25f);
}

void SuperweaponSystem::fireMissileBarrage(Vector2 origin, IGameWorld& world) {
    const std::vector<TargetInfo> targets =
        world.findEnemies(origin, BarrageTargetSearchRadius, BarrageMissileCount);

    for (int i = 0; i < BarrageMissileCount; ++i) {
        const float t = (BarrageMissileCount > 1)
            ? (static_cast<float>(i) / static_cast<float>(BarrageMissileCount - 1) - 0.5f)
            : 0.0f;
        const float launchAngle = t * BarrageFanRadians;

        ProjectileSpawn spawn;
        spawn.position = origin;
        // Missiles are aimed at distinct targets (wrapping when there are more
        // missiles than enemies) and then re-acquire for themselves; the
        // ProjectileSpawn contract carries no target handle, so aiming the
        // launch vector is how the assignment is expressed.
        if (!targets.empty()) {
            const TargetInfo& target = targets[static_cast<std::size_t>(i) % targets.size()];
            const float aim = angleOf(sub(target.position, origin));
            spawn.velocity = fromAngle(aim + launchAngle * 0.35f, BarrageLaunchSpeed);
        } else {
            spawn.velocity = fromAngle(launchAngle, BarrageLaunchSpeed);
        }
        spawn.damage = BarrageDamage;
        spawn.radius = BarrageRadius;
        spawn.size = BarrageSize;
        spawn.lifetime = BarrageLifetime;
        spawn.motion = ProjectileMotion::Homing;
        spawn.sprite = SpriteId::MissilePlayer;
        spawn.tint = BarrageTint;
        spawn.turnRate = BarrageTurnRate;
        spawn.acceleration = BarrageAcceleration;
        spawn.maxSpeed = BarrageMaxSpeed;
        spawn.emitsTrail = true;
        spawn.trailSprite = SpriteId::ParticleSmoke;
        world.spawnPlayerProjectile(spawn);
    }

    HU_LOG_INFO("Super", "Missile Barrage: %d missiles across %zu distinct targets",
                BarrageMissileCount, targets.size());
    world.addScreenShake(12.0f, 0.35f);
}

void SuperweaponSystem::beginLaserSpread(Vector2 origin, IGameWorld& world, WeaponSystem& weapons) {
    // Reuses the basic spread pattern so the two weapons always agree. A locked
    // spread weapon still gets the level-1 fan.
    const int spreadLevel = weapons.unlocked(WeaponType::Spread) ? weapons.level(WeaponType::Spread) : 1;
    m_beamAngles = WeaponSystem::spreadAngles(spreadLevel);
    m_beamHalfThickness = LaserSpreadHalfThickness;
    m_beamDamagePerSecond = LaserSpreadDamagePerSecond;
    m_duration = LaserSpreadDuration;
    m_timer = m_duration;

    HU_LOG_INFO("Super", "Laser Spread: %zu beams (spread L%d), %.1fs",
                m_beamAngles.size(), spreadLevel, static_cast<double>(m_duration));

    EffectRequest fx;
    fx.kind = EffectKind::SuperweaponCharge;
    fx.position = origin;
    fx.scale = 2.5f;
    fx.tint = BeamGlowTint;
    world.spawnEffect(fx);
    world.addScreenShake(BeamShakeIntensity, m_duration);
}

void SuperweaponSystem::beginHelixBeam(Vector2 origin, IGameWorld& world) {
    // Twice the thickness a level-5 basic laser reaches.
    m_beamAngles.assign(1, 0.0f);
    m_beamHalfThickness = WeaponSystem::maxLaserHalfThickness() * 2.0f;
    m_beamDamagePerSecond = HelixDamagePerSecond;
    m_duration = HelixDuration;
    m_timer = m_duration;
    m_helixSpawnTimer = 0.0f;
    m_helixEscortIndex = 0;

    HU_LOG_INFO("Super", "Helix Beam: half-thickness %.1f, %.1fs",
                static_cast<double>(m_beamHalfThickness), static_cast<double>(m_duration));

    EffectRequest fx;
    fx.kind = EffectKind::SuperweaponCharge;
    fx.position = origin;
    fx.scale = 3.0f;
    fx.tint = BeamGlowTint;
    world.spawnEffect(fx);
    world.addScreenShake(BeamShakeIntensity * 1.5f, m_duration);
}

void SuperweaponSystem::fireEnergyBomb(Vector2 origin, IGameWorld& world) {
    world.clearScreen(BombBossDamage);
    world.addScreenShake(BombShakeIntensity, BombShakeDuration);

    EffectRequest fx;
    fx.kind = EffectKind::ScreenClear;
    fx.position = origin;
    fx.scale = 1.0f;
    fx.tint = BombTint;
    world.spawnEffect(fx);

    EffectRequest blast;
    blast.kind = EffectKind::BigExplosion;
    blast.position = origin;
    blast.scale = 4.0f;
    blast.tint = BombTint;
    world.spawnEffect(blast);
}

// ---------------------------------------------------------------------------
// Dispatch and beam upkeep
// ---------------------------------------------------------------------------

void SuperweaponSystem::fire(SuperweaponType type, int cells, Vector2 origin,
                             IGameWorld& world, WeaponSystem& weapons) {
    m_active = type;
    m_beams.clear();
    m_pulse = 0.0f;

    switch (type) {
        case SuperweaponType::PiercingLance:
            firePiercingLance(origin, world);
            m_active = SuperweaponType::None;   // Instantaneous: no beam upkeep.
            break;
        case SuperweaponType::MissileBarrage:
            fireMissileBarrage(origin, world);
            m_active = SuperweaponType::None;
            break;
        case SuperweaponType::LaserSpread:
            beginLaserSpread(origin, world, weapons);
            break;
        case SuperweaponType::HelixBeam:
            beginHelixBeam(origin, world);
            break;
        case SuperweaponType::EnergyBomb:
            fireEnergyBomb(origin, world);
            m_active = SuperweaponType::None;
            break;
        default:
            m_active = SuperweaponType::None;
            return;
    }

    const std::vector<TargetInfo> onScreen = world.findEnemies(origin, 0.0f, 64);
    HU_LOG_INFO("Super", "FIRE %s: %d cell(s) consumed, %zu target(s) on screen",
                superweaponName(type), cells, onScreen.size());
}

void SuperweaponSystem::updateBeams(float deltaTime, Vector2 origin, IGameWorld& world) {
    const float reach = beamReach(world);
    m_beams.clear();
    m_beams.reserve(m_beamAngles.size());
    for (float angle : m_beamAngles) {
        BeamSegment beam;
        beam.origin = origin;
        beam.angle = angle;
        beam.length = reach;
        beam.halfThickness = m_beamHalfThickness;
        beam.damagePerSecond = m_beamDamagePerSecond;
        m_beams.push_back(beam);
    }

    m_pulse += deltaTime * BeamPulseRate;
    if (m_pulse > TwoPi) {
        m_pulse -= TwoPi;
    }

    if (m_active != SuperweaponType::HelixBeam) {
        return;
    }

    // Escort missiles spiral around the beam, then peel off to seek.
    m_helixSpawnTimer -= deltaTime;
    while (m_helixSpawnTimer <= 0.0f) {
        m_helixSpawnTimer += HelixEscortInterval;

        ProjectileSpawn spawn;
        spawn.position = origin;
        spawn.velocity = Vector2{ HelixEscortSpeed, 0.0f };
        spawn.damage = HelixEscortDamage;
        spawn.radius = HelixEscortRadius;
        spawn.size = HelixEscortSize;
        spawn.lifetime = HelixEscortLifetime;
        spawn.motion = ProjectileMotion::Orbiting;
        spawn.sprite = SpriteId::MissilePlayer;
        spawn.tint = BarrageTint;
        // For Orbiting motion Projectile reads turnRate as the starting orbit
        // phase; alternating it launches escorts on opposite sides of the beam.
        spawn.turnRate = (m_helixEscortIndex % 2 == 0) ? 0.0f : Pi;
        ++m_helixEscortIndex;
        spawn.maxSpeed = HelixEscortSpeed;
        spawn.emitsTrail = true;
        spawn.trailSprite = SpriteId::ParticleSpark;
        world.spawnPlayerProjectile(spawn);
    }
}

void SuperweaponSystem::update(float deltaTime, Vector2 shipPosition, IGameWorld& world,
                               WeaponSystem& weapons, EnergyCellSystem& cells) {
    const Vector2 origin = muzzleOf(shipPosition);

    if (m_fireRequested) {
        m_fireRequested = false;
        const SuperweaponType type = pending(cells);
        if (type == SuperweaponType::None) {
            HU_LOG_DEBUG("Super", "Fire refused: %d charged cell(s), active=%s",
                         cells.chargedCellCount(), superweaponName(m_active));
        } else {
            const int cellCount = static_cast<int>(type);
            if (cells.consumeCharge(cellCount)) {
                fire(type, cellCount, origin, world, weapons);
            }
        }
    }

    if (m_active == SuperweaponType::None) {
        m_beams.clear();
        return;
    }

    m_timer -= deltaTime;
    if (m_timer <= 0.0f) {
        HU_LOG_INFO("Super", "%s expired", superweaponName(m_active));
        m_active = SuperweaponType::None;
        m_timer = 0.0f;
        m_beams.clear();
        m_beamAngles.clear();
        return;
    }

    updateBeams(deltaTime, origin, world);
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void SuperweaponSystem::appendDraw(DrawList& out) const {
    if (m_beams.empty()) {
        return;
    }

    // Beams fade out over their final third so they do not pop off screen.
    float fade = 1.0f;
    if (m_duration > 0.0f) {
        const float remainingFraction = m_timer / m_duration;
        fade = clampf(remainingFraction * 3.0f, 0.0f, 1.0f);
    }
    const float pulse = (0.85f + 0.15f * std::sin(m_pulse)) * fade;

    for (const BeamSegment& beam : m_beams) {
        const Vector2 dir = fromAngle(beam.angle);
        const Vector2 centre = add(beam.origin, scale(dir, beam.length * 0.5f));

        Color glow = BeamGlowTint;
        glow.a *= pulse;
        out.add(SpriteId::LaserSegment, centre,
                Vector2{ beam.length, beam.halfThickness * 4.5f },
                DrawLayer::Beam, glow, beam.angle, true);

        Color core = BeamCoreTint;
        core.a = pulse;
        const int segments = static_cast<int>(beam.length / BeamSegmentQuadLength) + 1;
        const float segmentLength = beam.length / static_cast<float>(segments);
        for (int i = 0; i < segments; ++i) {
            const Vector2 pos = add(beam.origin,
                                    scale(dir, segmentLength * (static_cast<float>(i) + 0.5f)));
            out.add(SpriteId::LaserSegment, pos,
                    Vector2{ segmentLength, beam.halfThickness * 2.0f },
                    DrawLayer::Beam, core, beam.angle, true);
        }

        out.add(SpriteId::LaserHead, beam.origin,
                Vector2{ beam.halfThickness * 5.0f, beam.halfThickness * 5.0f },
                DrawLayer::Beam, core, beam.angle, true);
    }
}

} // namespace hu
