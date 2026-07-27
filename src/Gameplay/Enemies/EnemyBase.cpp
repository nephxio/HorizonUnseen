#include "Gameplay/Enemies/EnemyBase.h"

#include "Core/Log.h"
#include "Core/Math.h"
#include "Gameplay/IGameWorld.h"

#include <utility>

namespace hu {

// ---------------------------------------------------------------------------
// Tunables
// ---------------------------------------------------------------------------
namespace {

constexpr const char* kLogCategory = "Enemy";

// Sprite tint while the hit flash is active.
constexpr float kFlashRed = 1.0f;
constexpr float kFlashGreen = 1.0f;
constexpr float kFlashBlue = 1.0f;

// Explosion scale is derived from the collision radius so a Splitter pops
// harder than a Drifter without per-archetype bookkeeping.
constexpr float kExplosionScaleDivisor = 18.0f;
constexpr float kExplosionScaleMin = 0.6f;
constexpr float kExplosionScaleMax = 3.0f;

} // namespace

// ---------------------------------------------------------------------------
// IEnemyBehavior defaults
// ---------------------------------------------------------------------------

void IEnemyBehavior::onSpawn(EnemyBase&, const EnemySpawnParams&) {}
void IEnemyBehavior::onDeath(EnemyBase&, IGameWorld&) {}
void IEnemyBehavior::appendDraw(const EnemyBase&, DrawList&) const {}

// ---------------------------------------------------------------------------
// EnemyBase
// ---------------------------------------------------------------------------

EnemyBase::EnemyBase(EnemyArchetype archetype,
                     const EnemySpawnParams& params,
                     std::unique_ptr<IEnemyBehavior> behavior)
    : m_archetype(archetype)
    , m_behavior(std::move(behavior))
    , m_position(params.position)
    , m_velocity(params.initialVelocity)
    , m_phase(params.phase)
    , m_speedScale(params.speedScale)
    , m_worldScrollSpeed(params.worldScrollSpeed)
    , m_lifetime(params.lifetime) {
    if (m_behavior) {
        m_behavior->onSpawn(*this, params);
    }
}

EnemyBase::~EnemyBase() = default;

float EnemyBase::healthFraction() const {
    if (m_maxHitPoints <= 0.0f) {
        return 0.0f;
    }
    return clampf(m_hitPoints / m_maxHitPoints, 0.0f, 1.0f);
}

void EnemyBase::setMaxHitPoints(float value) {
    m_maxHitPoints = value > 0.0f ? value : 1.0f;
    m_hitPoints = m_maxHitPoints;
}

void EnemyBase::advanceTimers(float dt) {
    m_age += dt;
    if (m_hitFlash > 0.0f) {
        m_hitFlash -= dt;
    }
    if (m_invulnerable > 0.0f) {
        m_invulnerable -= dt;
    }
}

void EnemyBase::integrate(float dt, IGameWorld& world) {
    m_position.x += m_velocity.x * dt;
    m_position.y += m_velocity.y * dt;
    updateCulling(world);
}

void EnemyBase::updateCulling(const IGameWorld& world) {
    const float w = world.screenWidth();
    const float h = world.screenHeight();
    m_offScreen = m_position.x < -EnemyCullMarginLeft ||
                  m_position.x > w + EnemyCullMarginRight ||
                  m_position.y < -EnemyCullMarginVertical ||
                  m_position.y > h + EnemyCullMarginVertical;
}

void EnemyBase::update(float dt, IGameWorld& world) {
    if (!m_alive) {
        return;
    }

    advanceTimers(dt);

    if (m_lifetime > 0.0f) {
        m_lifetime -= dt;
        if (m_lifetime <= 0.0f) {
            // Expiring minions still pop, but do not count as a player kill.
            die(world, false);
            return;
        }
    }

    if (m_behavior) {
        m_behavior->update(*this, dt, world);
    }

    integrate(dt, world);
}

Color EnemyBase::renderTint() const {
    if (m_hitFlash <= 0.0f) {
        return m_tint;
    }
    const float t = clampf(m_hitFlash / EnemyHitFlashDuration, 0.0f, 1.0f);
    Color out;
    out.r = lerp(m_tint.r, kFlashRed, t);
    out.g = lerp(m_tint.g, kFlashGreen, t);
    out.b = lerp(m_tint.b, kFlashBlue, t);
    out.a = m_tint.a;
    return out;
}

void EnemyBase::appendDraw(DrawList& out) const {
    if (!m_alive) {
        return;
    }
    out.add(m_sprite, m_position, m_size, DrawLayer::Enemy, renderTint(), m_rotation);
    if (m_behavior) {
        m_behavior->appendDraw(*this, out);
    }
}

void EnemyBase::takeDamage(float amount, IGameWorld& world) {
    if (!m_alive || amount <= 0.0f) {
        return;
    }
    if (isInvulnerable()) {
        return;
    }

    m_hitPoints -= amount;
    m_hitFlash = EnemyHitFlashDuration;

    if (m_hitPoints <= 0.0f) {
        m_hitPoints = 0.0f;
        die(world, true);
    }
}

void EnemyBase::destroy(IGameWorld& world) {
    if (!m_alive) {
        return;
    }
    m_hitPoints = 0.0f;
    die(world, true);
}

void EnemyBase::despawn() {
    m_alive = false;
    m_offScreen = true;
    m_hasDeathEvent = false;
}

void EnemyBase::die(IGameWorld& world, bool killedByPlayer) {
    if (!m_alive) {
        return;
    }
    m_alive = false;

    // Behaviour first: Splitter needs to queue children and Mine needs to fire
    // its ring while the position is still valid.
    if (m_behavior) {
        m_behavior->onDeath(*this, world);
    }

    EffectRequest boom;
    boom.kind = isBoss() ? EffectKind::BigExplosion : EffectKind::Explosion;
    boom.position = m_position;
    boom.direction = normalize(m_velocity);
    boom.scale = clampf(m_radius / kExplosionScaleDivisor, kExplosionScaleMin, kExplosionScaleMax);
    boom.tint = m_tint;
    world.spawnEffect(boom);

    m_deathEvent.archetype = m_archetype;
    m_deathEvent.position = m_position;
    m_deathEvent.velocity = m_velocity;
    m_deathEvent.wasBoss = isBoss();
    m_deathEvent.killedByPlayer = killedByPlayer;
    m_deathEvent.isMinion = m_minion;
    m_deathEvent.scoreValue = m_scoreValue;
    m_hasDeathEvent = true;

    HU_LOG_DEBUG(kLogCategory, "%s destroyed at (%.0f, %.0f) killedByPlayer=%d",
                 enemyArchetypeName(m_archetype), m_position.x, m_position.y,
                 killedByPlayer ? 1 : 0);
}

void EnemyBase::spawnChild(std::unique_ptr<EnemyBase> child) {
    if (child) {
        m_pendingSpawns.push_back(std::move(child));
    }
}

std::vector<std::unique_ptr<EnemyBase>> EnemyBase::takePendingSpawns() {
    std::vector<std::unique_ptr<EnemyBase>> out = std::move(m_pendingSpawns);
    m_pendingSpawns.clear();
    return out;
}

} // namespace hu
