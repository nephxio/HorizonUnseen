#include "Gameplay/Enemies/Boss.h"

#include "Core/Log.h"
#include "Core/Math.h"
#include "Gameplay/Enemies/EnemyBehaviors.h"
#include "Gameplay/IGameWorld.h"

#include <cmath>
#include <memory>

namespace hu {

// ===========================================================================
// TUNABLES
// ===========================================================================
namespace {

constexpr const char* kLogCategory = "Boss";

// --- Phase thresholds (fraction of max HP) ---------------------------------
constexpr float kPhase2Threshold = 0.66f;
constexpr float kPhase3Threshold = 0.33f;

// --- Entrance ---------------------------------------------------------------
constexpr float kEntryX = 980.0f;          // Where the boss settles.
constexpr float kEntrySpeed = 190.0f;
constexpr float kEntryGraceSeconds = 1.2f; // Invulnerable while sliding in.

// --- Transitions ------------------------------------------------------------
constexpr float kTransitionDuration = 1.5f;
constexpr float kTransitionShakeIntensity = 0.9f;
constexpr float kTransitionShakeDuration = 0.6f;

// --- Phase 1: Sentinel ------------------------------------------------------
constexpr float kSentinelHoverAmplitude = 150.0f;
constexpr float kSentinelHoverRate = 0.75f;
constexpr float kSentinelDriftX = 26.0f;    // Gentle in/out breathing.
constexpr float kSentinelVolleyInterval = 2.3f;
constexpr float kSentinelVolleyIntervalHell = 1.25f;
constexpr int kSentinelVolleyShots = 3;
constexpr int kSentinelVolleyShotsHell = 5;
constexpr float kSentinelBurstSpacing = 0.14f;
constexpr float kSentinelFanCount = 3;
constexpr float kSentinelFanCountHell = 5;
constexpr float kSentinelFanSpread = 0.34f;
constexpr float kSentinelBulletSpeed = 330.0f;
constexpr float kSentinelBulletSpeedHell = 420.0f;
constexpr float kSentinelBulletDamage = 12.0f;

// --- Phase 2: Weaver --------------------------------------------------------
constexpr float kWeaverPathRate = 0.62f;
constexpr float kWeaverAmplitudeX = 210.0f;
constexpr float kWeaverAmplitudeY = 200.0f;
constexpr float kWeaverCentreX = 900.0f;
constexpr float kWeaverSpiralInterval = 0.1f;
constexpr float kWeaverSpiralIntervalHell = 0.055f;
constexpr int kWeaverSpiralArms = 3;
constexpr int kWeaverSpiralArmsHell = 5;
constexpr float kWeaverSpiralStep = 0.41f;   // Radians added per emission.
constexpr float kWeaverBulletSpeed = 250.0f;
constexpr float kWeaverBulletSpeedHell = 320.0f;
constexpr float kWeaverBulletDamage = 10.0f;

// --- Phase 3: Bulwark -------------------------------------------------------
constexpr float kBulwarkX = 1010.0f;
constexpr float kBulwarkSweepSpeed = 185.0f;
constexpr float kBulwarkTopMargin = 130.0f;
constexpr float kBulwarkWallInterval = 1.5f;
constexpr float kBulwarkWallIntervalHell = 0.95f;
constexpr int kBulwarkWallRows = 13;
constexpr float kBulwarkWallSpeed = 210.0f;
constexpr float kBulwarkWallSpeedHell = 265.0f;
constexpr float kBulwarkWallDamage = 14.0f;
constexpr float kBulwarkGapWidth = 1.4f;     // In row units, half-width.
constexpr float kBulwarkGapDriftRate = 0.33f;
constexpr float kBulwarkSnipeInterval = 1.1f;
constexpr float kBulwarkSnipeIntervalHell = 0.6f;
constexpr float kBulwarkSnipeSpeed = 430.0f;
constexpr float kBulwarkSnipeDamage = 16.0f;

// --- Presentation -----------------------------------------------------------
constexpr float kCoreGlowBase = 46.0f;
constexpr float kCoreGlowPulseRate = 3.4f;

float byMode(const IGameWorld& world, float normal, float hell) {
    return world.difficulty() == DifficultyMode::BulletHell ? hell : normal;
}

} // namespace

const char* bossPhaseName(BossPhase phase) {
    switch (phase) {
        case BossPhase::Entering: return "Entering";
        case BossPhase::Sentinel: return "Phase 1 - Sentinel";
        case BossPhase::Weaver:   return "Phase 2 - Weaver";
        case BossPhase::Bulwark:  return "Phase 3 - Bulwark";
        case BossPhase::Dying:    return "Dying";
        default:                  return "Unknown";
    }
}

// ---------------------------------------------------------------------------

Boss::Boss(const EnemySpawnParams& params)
    : EnemyBase(EnemyArchetype::Boss, params, nullptr) {
    m_home = params.position;
    m_sprite = SpriteId::EnemyBoss;
    m_size = { 190.0f, 150.0f };
    m_radius = 74.0f;
    m_contactDamage = 60.0f;
    m_scoreValue = 10000;
    // Untouchable during the entrance so it cannot be melted before it arrives.
    setInvulnerable(kEntryGraceSeconds);
    HU_LOG_INFO(kLogCategory, "Boss engaged: %s", bossPhaseName(m_phase));
}

int Boss::phaseIndex() const {
    switch (m_phase) {
        case BossPhase::Sentinel: return 1;
        case BossPhase::Weaver:   return 2;
        case BossPhase::Bulwark:  return 3;
        default:                  return 0;
    }
}

void Boss::enterPhase(BossPhase next, IGameWorld& world) {
    const BossPhase previous = m_phase;
    m_phase = next;
    m_phaseTime = 0.0f;
    m_fireTimer = 0.0f;
    m_burstTimer = 0.0f;
    m_burstLeft = 0;
    m_spiralAngle = 0.0f;

    if (next != BossPhase::Entering) {
        m_transitionTimer = kTransitionDuration;
        setInvulnerable(kTransitionDuration);

        EffectRequest fx;
        fx.kind = EffectKind::BigExplosion;
        fx.position = position();
        fx.scale = 2.2f;
        fx.tint = Color{ 1.0f, 0.65f, 0.25f, 1.0f };
        world.spawnEffect(fx);

        EffectRequest charge;
        charge.kind = EffectKind::SuperweaponCharge;
        charge.position = position();
        charge.scale = 2.6f;
        charge.tint = Color{ 0.6f, 0.85f, 1.0f, 1.0f };
        world.spawnEffect(charge);

        world.addScreenShake(kTransitionShakeIntensity, kTransitionShakeDuration);
    }

    HU_LOG_INFO(kLogCategory, "Boss phase change: %s -> %s (HP %.0f / %.0f, %.0f%%)",
                bossPhaseName(previous), bossPhaseName(next), hitPoints(), maxHitPoints(),
                healthFraction() * 100.0f);
}

void Boss::checkPhaseTransition(IGameWorld& world) {
    const float hp = healthFraction();
    if (m_phase == BossPhase::Sentinel && hp <= kPhase2Threshold) {
        enterPhase(BossPhase::Weaver, world);
    } else if (m_phase == BossPhase::Weaver && hp <= kPhase3Threshold) {
        enterPhase(BossPhase::Bulwark, world);
    }
}

void Boss::update(float dt, IGameWorld& world) {
    if (!isAlive()) {
        return;
    }

    advanceTimers(dt);
    m_phaseTime += dt;
    m_pathPhase += dt;
    m_screenHeight = world.screenHeight();

    if (m_transitionTimer > 0.0f) {
        m_transitionTimer -= dt;
        // Hold position and hold fire while the transition plays out.
        setVelocity(scale(velocity(), 0.86f));
        integrate(dt, world);
        return;
    }

    switch (m_phase) {
        case BossPhase::Entering: updateEntering(dt, world); break;
        case BossPhase::Sentinel: updateSentinel(dt, world); break;
        case BossPhase::Weaver:   updateWeaver(dt, world);   break;
        case BossPhase::Bulwark:  updateBulwark(dt, world);  break;
        default: break;
    }

    integrate(dt, world);
    checkPhaseTransition(world);
}

// --- Entrance ---------------------------------------------------------------

void Boss::updateEntering(float dt, IGameWorld& world) {
    (void)dt;
    setVelocity({ -kEntrySpeed, 0.0f });
    if (position().x <= kEntryX) {
        Vector2 p = position();
        p.x = kEntryX;
        setPosition(p);
        m_home = { kEntryX, world.screenHeight() * 0.5f };
        enterPhase(BossPhase::Sentinel, world);
    }
}

// --- Phase 1: Sentinel ------------------------------------------------------
//   Movement: slow vertical hover with a shallow in/out breath.
//   Weapon:   a burst of aimed fans -- readable, punishes standing still.

void Boss::updateSentinel(float dt, IGameWorld& world) {
    const float hoverY = m_home.y + std::sin(m_pathPhase * TwoPi * kSentinelHoverRate * 0.25f) *
                                        kSentinelHoverAmplitude;
    const float hoverX = kEntryX + std::sin(m_pathPhase * 0.9f) * kSentinelDriftX;
    const Vector2 target{ hoverX, hoverY };
    setVelocity(scale(sub(target, position()), 2.4f));

    if (m_burstLeft > 0) {
        m_burstTimer -= dt;
        if (m_burstTimer <= 0.0f) {
            m_burstTimer = kSentinelBurstSpacing;
            --m_burstLeft;

            enemyfire::Shot shot;
            shot.speed = byMode(world, kSentinelBulletSpeed, kSentinelBulletSpeedHell);
            shot.damage = kSentinelBulletDamage;
            shot.sprite = SpriteId::BulletEnemyHeavy;
            shot.radius = 7.0f;
            shot.size = { 18.0f, 11.0f };

            const Vector2 origin = add(position(), Vector2{ -size().x * 0.4f, 0.0f });
            const Vector2 dir = enemyfire::aimAtPlayer(world, origin);
            const int fan = static_cast<int>(byMode(world, kSentinelFanCount, kSentinelFanCountHell));
            enemyfire::fireFan(world, origin, dir, fan, kSentinelFanSpread, shot);
            enemyfire::muzzle(world, origin, dir, 1.2f);
        }
        return;
    }

    m_fireTimer -= dt;
    if (m_fireTimer <= 0.0f) {
        m_fireTimer = byMode(world, kSentinelVolleyInterval, kSentinelVolleyIntervalHell);
        m_burstLeft = static_cast<int>(
            byMode(world, static_cast<float>(kSentinelVolleyShots),
                   static_cast<float>(kSentinelVolleyShotsHell)));
        m_burstTimer = 0.0f;
    }
}

// --- Phase 2: Weaver --------------------------------------------------------
//   Movement: lissajous figure-eight across the right half of the screen.
//   Weapon:   continuously rotating spiral arms -- a moving maze rather than a
//             set of discrete threats.

void Boss::updateWeaver(float dt, IGameWorld& world) {
    const float t = m_phaseTime * kWeaverPathRate * TwoPi;
    const Vector2 target{ kWeaverCentreX + std::sin(t) * kWeaverAmplitudeX * 0.5f,
                          world.screenHeight() * 0.5f + std::sin(t * 2.0f) * kWeaverAmplitudeY };
    setVelocity(scale(sub(target, position()), 3.0f));

    m_fireTimer -= dt;
    if (m_fireTimer <= 0.0f) {
        m_fireTimer = byMode(world, kWeaverSpiralInterval, kWeaverSpiralIntervalHell);
        m_spiralAngle += kWeaverSpiralStep;

        enemyfire::Shot shot;
        shot.speed = byMode(world, kWeaverBulletSpeed, kWeaverBulletSpeedHell);
        shot.damage = kWeaverBulletDamage;
        shot.additive = true;
        shot.tint = Color{ 0.65f, 0.85f, 1.0f, 1.0f };

        const int arms = static_cast<int>(byMode(world,
                                                 static_cast<float>(kWeaverSpiralArms),
                                                 static_cast<float>(kWeaverSpiralArmsHell)));
        enemyfire::fireRing(world, position(), arms, m_spiralAngle, shot);
    }
}

// --- Phase 3: Bulwark -------------------------------------------------------
//   Movement: parks near the right edge and sweeps the full height of the
//             screen, forcing the player to keep moving.
//   Weapon:   a dense wall of bullets with exactly one gap that drifts, plus
//             aimed snipes to punish camping in the gap.

void Boss::updateBulwark(float dt, IGameWorld& world) {
    const float top = kBulwarkTopMargin;
    const float bottom = world.screenHeight() - kBulwarkTopMargin;

    Vector2 p = position();
    if (p.y <= top && m_sweepDirection < 0) {
        m_sweepDirection = 1;
    } else if (p.y >= bottom && m_sweepDirection > 0) {
        m_sweepDirection = -1;
    }
    const float vy = kBulwarkSweepSpeed * static_cast<float>(m_sweepDirection);
    setVelocity({ (kBulwarkX - p.x) * 2.0f, vy });

    // The gap wanders so the player cannot pre-position for it.
    m_wallGap += m_wallGapDrift * kBulwarkGapDriftRate * dt;
    if (m_wallGap > 1.0f) { m_wallGap = 1.0f; m_wallGapDrift = -1.0f; }
    if (m_wallGap < 0.0f) { m_wallGap = 0.0f; m_wallGapDrift = 1.0f; }

    m_fireTimer -= dt;
    if (m_fireTimer <= 0.0f) {
        m_fireTimer = byMode(world, kBulwarkWallInterval, kBulwarkWallIntervalHell);

        enemyfire::Shot wallShot;
        wallShot.speed = byMode(world, kBulwarkWallSpeed, kBulwarkWallSpeedHell);
        wallShot.damage = kBulwarkWallDamage;
        wallShot.radius = 8.0f;
        wallShot.size = { 20.0f, 20.0f };
        wallShot.tint = Color{ 1.0f, 0.55f, 0.45f, 1.0f };

        const float spacing = world.screenHeight() / static_cast<float>(kBulwarkWallRows - 1);
        const float gapRow = m_wallGap * static_cast<float>(kBulwarkWallRows - 1);
        const float originX = position().x - size().x * 0.45f;
        for (int row = 0; row < kBulwarkWallRows; ++row) {
            if (std::fabs(static_cast<float>(row) - gapRow) <= kBulwarkGapWidth) {
                continue;   // The gap: the player's only way through.
            }
            const Vector2 origin{ originX, spacing * static_cast<float>(row) };
            enemyfire::fire(world, origin, Vector2{ -1.0f, 0.0f }, wallShot);
        }
        world.addScreenShake(0.2f, 0.15f);
    }

    m_burstTimer -= dt;
    if (m_burstTimer <= 0.0f) {
        m_burstTimer = byMode(world, kBulwarkSnipeInterval, kBulwarkSnipeIntervalHell);

        enemyfire::Shot snipe;
        snipe.speed = kBulwarkSnipeSpeed;
        snipe.damage = kBulwarkSnipeDamage;
        snipe.sprite = SpriteId::BulletEnemyHeavy;
        snipe.radius = 6.0f;
        snipe.size = { 22.0f, 10.0f };

        const Vector2 origin = add(position(), Vector2{ -size().x * 0.4f, 0.0f });
        enemyfire::fire(world, origin, enemyfire::aimAtPlayer(world, origin), snipe);
    }
}

// ---------------------------------------------------------------------------

void Boss::appendDraw(DrawList& out) const {
    if (!isAlive()) {
        return;
    }

    EnemyBase::appendDraw(out);

    // Core glow: brightens as phases escalate, strobes during a transition.
    const float phaseBoost = 0.25f * static_cast<float>(phaseIndex());
    const float pulse = 0.5f + 0.5f * std::sin(m_pathPhase * kCoreGlowPulseRate);
    float alpha = 0.22f + phaseBoost * 0.4f + pulse * 0.12f;
    Color glow{ 1.0f, 0.55f + phaseBoost, 0.35f, alpha };

    if (m_transitionTimer > 0.0f) {
        const float strobe = 0.5f + 0.5f * std::sin(m_transitionTimer * 30.0f);
        glow = Color{ 0.7f + 0.3f * strobe, 0.9f, 1.0f, 0.35f + 0.45f * strobe };
    }

    const float s = kCoreGlowBase * (1.0f + 0.35f * pulse) * (1.0f + phaseBoost);
    out.add(SpriteId::ParticleGlow, position(), { s, s }, DrawLayer::Particle, glow, 0.0f, true);

    // Phase 3 telegraph: a thin bar marking where the wall's gap will open.
    if (m_phase == BossPhase::Bulwark && m_transitionTimer <= 0.0f) {
        const float gapY = m_wallGap * m_screenHeight;
        Color marker{ 0.4f, 1.0f, 0.6f, 0.16f };
        out.add(SpriteId::White, Vector2{ position().x - size().x * 0.45f, gapY },
                Vector2{ 26.0f, 74.0f }, DrawLayer::EnemyTrail, marker, 0.0f, true);
    }
}

} // namespace hu
