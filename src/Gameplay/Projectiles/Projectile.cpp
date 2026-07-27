#include "Gameplay/Projectiles/Projectile.h"

#include "Core/Log.h"
#include "Core/Math.h"

namespace hu {

// ---------------------------------------------------------------------------
// Tuning constants (this file's single tunable block).
// ---------------------------------------------------------------------------
namespace {

// How far outside the screen a projectile may drift before it is culled.
constexpr float CullMargin = 96.0f;

// Homing missiles that lose their lock retry an acquisition on this cadence
// rather than every frame, which keeps the nearest-enemy query cheap.
constexpr float ReacquireInterval = 0.12f;

// Search radius used when a homing round looks for a new target.
constexpr float HomingSearchRadius = 1400.0f;

// Orbiting escorts. ProjectileSpawn has no orbit fields, so `turnRate` is
// reinterpreted as the initial orbit phase in radians for Orbiting motion
// only -- documented here because it is the one place a spawn field changes
// meaning by motion mode.
constexpr float OrbitRadius = 34.0f;
constexpr float OrbitAngularRate = 7.0f;      // radians/second
constexpr float OrbitPeelFraction = 0.45f;    // of lifetime, then it seeks
constexpr float OrbitHomingTurnRate = 5.0f;   // radians/second after peeling

} // namespace

void Projectile::initialize(const ProjectileSpawn& spawn, Faction faction) {
    m_position = spawn.position;
    m_velocity = spawn.velocity;
    m_size = spawn.size;
    m_tint = spawn.tint;
    m_sprite = spawn.sprite;
    m_trailSprite = spawn.trailSprite;
    m_motion = spawn.motion;
    m_faction = faction;

    m_damage = spawn.damage;
    m_radius = spawn.radius;
    m_lifetime = spawn.lifetime;
    m_age = 0.0f;
    m_turnRate = spawn.turnRate;
    m_acceleration = spawn.acceleration;
    m_maxSpeed = spawn.maxSpeed;
    m_rotation = angleOf(spawn.velocity);

    m_pierceRemaining = spawn.pierceCount > 0 ? spawn.pierceCount : 1;
    m_additive = spawn.additive;
    m_emitsTrail = spawn.emitsTrail;
    m_alive = true;

    m_targetHandle = InvalidTarget;
    m_reacquireTimer = 0.0f;

    if (m_motion == ProjectileMotion::Orbiting) {
        m_orbitAnchor = spawn.position;
        m_orbitAxis = normalize(spawn.velocity);
        if (lengthSquared(m_orbitAxis) <= 0.0f) {
            m_orbitAxis = Vector2{ 1.0f, 0.0f };
        }
        m_orbitPhase = spawn.turnRate;   // See note above: phase, not turn rate.
        m_orbitSpeed = length(spawn.velocity);
        m_turnRate = OrbitHomingTurnRate;
    }
}

void Projectile::kill() {
    m_alive = false;
}

bool Projectile::onHit() {
    if (!m_alive) {
        return true;
    }
    --m_pierceRemaining;
    if (m_pierceRemaining <= 0) {
        m_alive = false;
        return true;
    }
    return false;
}

bool Projectile::offWorld(const IGameWorld& world) const {
    const float w = world.screenWidth();
    const float h = world.screenHeight();
    return m_position.x < -CullMargin || m_position.x > w + CullMargin ||
           m_position.y < -CullMargin || m_position.y > h + CullMargin;
}

void Projectile::updateHoming(float deltaTime, IGameWorld& world) {
    TargetInfo target;
    bool haveTarget = false;

    if (m_targetHandle != InvalidTarget && world.resolveTarget(m_targetHandle, target)) {
        haveTarget = true;
    } else {
        // Lock lost: drop it and try to re-acquire on the next retry tick.
        m_targetHandle = InvalidTarget;
        m_reacquireTimer -= deltaTime;
        if (m_reacquireTimer <= 0.0f) {
            m_reacquireTimer = ReacquireInterval;
            if (world.findNearestEnemy(m_position, HomingSearchRadius, target)) {
                m_targetHandle = target.handle;
                haveTarget = true;
            }
        }
    }

    // turnRate == 0 means "no steering" even for Homing motion, which is how a
    // missile is told to fly straight.
    if (haveTarget && m_turnRate > 0.0f) {
        const float desired = angleOf(sub(target.position, m_position));
        m_rotation = rotateTowards(m_rotation, desired, m_turnRate * deltaTime);
    }

    float speed = length(m_velocity);
    speed += m_acceleration * deltaTime;
    if (m_maxSpeed > 0.0f && speed > m_maxSpeed) {
        speed = m_maxSpeed;
    }
    m_velocity = fromAngle(m_rotation, speed);
}

void Projectile::updateOrbiting(float deltaTime, IGameWorld& world) {
    // The anchor is the imaginary beam core; the missile swings across it.
    m_orbitAnchor = add(m_orbitAnchor, scale(m_orbitAxis, m_orbitSpeed * deltaTime));
    m_orbitPhase += OrbitAngularRate * deltaTime;

    const Vector2 perpendicular{ -m_orbitAxis.y, m_orbitAxis.x };
    const Vector2 previous = m_position;
    m_position = add(m_orbitAnchor, scale(perpendicular, std::sin(m_orbitPhase) * OrbitRadius));

    const Vector2 step = sub(m_position, previous);
    if (deltaTime > 0.0f) {
        m_velocity = scale(step, 1.0f / deltaTime);
    }
    m_rotation = angleOf(m_velocity);

    // Peel off part-way through life and become a normal seeker.
    if (m_lifetime > 0.0f && m_age >= m_lifetime * OrbitPeelFraction) {
        m_motion = ProjectileMotion::Homing;
        m_turnRate = OrbitHomingTurnRate;
        m_velocity = fromAngle(m_rotation, m_orbitSpeed);
        HU_LOG_TRACE("Projectile", "Helix escort peeled off to seek");
        (void)world;
    }
}

void Projectile::update(float deltaTime, IGameWorld& world) {
    if (!m_alive) {
        return;
    }

    m_age += deltaTime;
    if (m_lifetime > 0.0f && m_age >= m_lifetime) {
        m_alive = false;
        return;
    }

    switch (m_motion) {
        case ProjectileMotion::Homing:
            updateHoming(deltaTime, world);
            m_position = add(m_position, scale(m_velocity, deltaTime));
            break;

        case ProjectileMotion::Orbiting:
            updateOrbiting(deltaTime, world);
            break;

        case ProjectileMotion::Straight:
        case ProjectileMotion::Piercing:
        default: {
            float speed = length(m_velocity);
            if (m_acceleration != 0.0f && speed > 0.0f) {
                speed += m_acceleration * deltaTime;
                if (m_maxSpeed > 0.0f && speed > m_maxSpeed) {
                    speed = m_maxSpeed;
                }
                m_velocity = fromAngle(m_rotation, speed);
            }
            m_position = add(m_position, scale(m_velocity, deltaTime));
            break;
        }
    }

    if (offWorld(world)) {
        m_alive = false;
    }
}

void Projectile::appendDraw(DrawList& out) const {
    if (!m_alive) {
        return;
    }
    const DrawLayer layer = (m_faction == Faction::Player) ? DrawLayer::PlayerProjectile
                                                           : DrawLayer::EnemyProjectile;
    out.add(m_sprite, m_position, m_size, layer, m_tint, m_rotation, m_additive);
}

} // namespace hu
