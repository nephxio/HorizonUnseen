#pragma once

// Base class for every hostile in the game.
//
// EnemyBase owns the state that all enemies share (transform, hit points,
// collision, hit flash, death bookkeeping) and delegates the interesting part
// -- how it moves and how it shoots -- to a pluggable IEnemyBehavior. That
// keeps update() free of a giant per-archetype switch and lets a behaviour hold
// its own private state (wave phase, dive stage, orbit angle) without bloating
// the base class.
//
// Enemies never roll their own drops. When one dies it publishes an
// EnemyDeathEvent; the scene drains that and hands it to PowerupSystem.

#include "Core/DrawList.h"
#include "Core/GameTypes.h"
#include "Core/SpriteId.h"
#include "Core/Vector2.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace hu {

class IGameWorld;
class EnemyBase;

// ---------------------------------------------------------------------------
// Tunables
// ---------------------------------------------------------------------------

// How long an enemy renders white after being hit.
inline constexpr float EnemyHitFlashDuration = 0.09f;

// Cull margins around the play field. The right margin is generous because
// several archetypes (Diver, Boss) legitimately loiter off the right edge.
inline constexpr float EnemyCullMarginLeft = 180.0f;
inline constexpr float EnemyCullMarginRight = 640.0f;
inline constexpr float EnemyCullMarginVertical = 460.0f;

// Default x used by the level data when an enemy should enter from the right.
inline constexpr float EnemySpawnX = 1340.0f;

// ---------------------------------------------------------------------------
// Spawn description
// ---------------------------------------------------------------------------

// Everything the factory needs to stamp out one enemy. Levels fill this in from
// their WaveSpawn rows; behaviours read the fields that apply to them and
// ignore the rest.
struct EnemySpawnParams {
    Vector2 position{ EnemySpawnX, 360.0f };
    Vector2 initialVelocity{ 0.0f, 0.0f };

    // Wave phase (WaveRider), orbit start angle (Orbiter), fire-cadence offset
    // (everything else). Radians.
    float phase = 0.0f;

    // Multiplies the archetype's nominal movement speed.
    float speedScale = 1.0f;

    // Index within a formation. WaveRiders use it to stagger their phase so a
    // squadron snakes in sequence.
    int formationIndex = 0;

    // Movement amplitude override in pixels; 0 means "use the archetype default".
    float amplitude = 0.0f;

    // Multiplies the archetype's nominal hit points.
    float healthScale = 1.0f;

    // Seconds before self-expiry; 0 means unlimited. Splitter children use it.
    float lifetime = 0.0f;

    // The level's scroll speed, so world-anchored enemies (Turret, Mine) can
    // ride the background exactly.
    float worldScrollSpeed = 120.0f;
};

// ---------------------------------------------------------------------------
// Death reporting
// ---------------------------------------------------------------------------

struct EnemyDeathEvent {
    EnemyArchetype archetype = EnemyArchetype::Drifter;
    Vector2 position{ 0.0f, 0.0f };
    Vector2 velocity{ 0.0f, 0.0f };
    bool wasBoss = false;
    bool killedByPlayer = true;
    // Splitter children and other spawned minions should not roll the full
    // drop table; the scene can use this to weight the roll down.
    bool isMinion = false;
    int scoreValue = 0;
};

// ---------------------------------------------------------------------------
// Behaviour interface
// ---------------------------------------------------------------------------

class IEnemyBehavior {
public:
    virtual ~IEnemyBehavior() = default;

    // Called once, immediately after the enemy is constructed.
    virtual void onSpawn(EnemyBase& self, const EnemySpawnParams& params);

    // Steering + weapons. Set velocity (or position directly) on `self`; the
    // base class integrates afterwards.
    virtual void update(EnemyBase& self, float dt, IGameWorld& world) = 0;

    // Last chance to spawn children, detonate, or emit effects.
    virtual void onDeath(EnemyBase& self, IGameWorld& world);

    // Extra decoration drawn on top of the enemy sprite.
    virtual void appendDraw(const EnemyBase& self, DrawList& out) const;

    virtual const char* name() const = 0;
};

// ---------------------------------------------------------------------------
// EnemyBase
// ---------------------------------------------------------------------------

class EnemyBase {
public:
    EnemyBase(EnemyArchetype archetype,
              const EnemySpawnParams& params,
              std::unique_ptr<IEnemyBehavior> behavior);
    virtual ~EnemyBase();

    EnemyBase(const EnemyBase&) = delete;
    EnemyBase& operator=(const EnemyBase&) = delete;

    // --- Frame ------------------------------------------------------------
    virtual void update(float dt, IGameWorld& world);
    virtual void appendDraw(DrawList& out) const;

    // --- Damage / death ---------------------------------------------------
    void takeDamage(float amount, IGameWorld& world);
    // Immediate kill that still reports a death event (player kill, contact).
    void destroy(IGameWorld& world);
    // Silent removal with no death event and no drop roll (off-screen cull).
    void despawn();

    // --- State ------------------------------------------------------------
    bool isAlive() const { return m_alive; }
    bool isOffScreen() const { return m_offScreen; }
    virtual bool isBoss() const { return m_archetype == EnemyArchetype::Boss; }
    bool isInvulnerable() const { return m_invulnerable > 0.0f; }
    bool isMinion() const { return m_minion; }

    EnemyArchetype archetype() const { return m_archetype; }

    Vector2 position() const { return m_position; }
    void setPosition(Vector2 value) { m_position = value; }

    Vector2 velocity() const { return m_velocity; }
    void setVelocity(Vector2 value) { m_velocity = value; }

    float rotation() const { return m_rotation; }
    void setRotation(float radians) { m_rotation = radians; }

    float radius() const { return m_radius; }
    void setRadius(float value) { m_radius = value; }

    Vector2 size() const { return m_size; }
    void setSize(Vector2 value) { m_size = value; }

    SpriteId sprite() const { return m_sprite; }
    void setSprite(SpriteId value) { m_sprite = value; }

    Color tint() const { return m_tint; }
    void setTint(Color value) { m_tint = value; }

    float hitPoints() const { return m_hitPoints; }
    float maxHitPoints() const { return m_maxHitPoints; }
    float healthFraction() const;
    void setMaxHitPoints(float value);
    void setHitPoints(float value) { m_hitPoints = value; }

    float contactDamage() const { return m_contactDamage; }
    void setContactDamage(float value) { m_contactDamage = value; }

    int scoreValue() const { return m_scoreValue; }
    void setScoreValue(int value) { m_scoreValue = value; }

    float age() const { return m_age; }
    float hitFlash() const { return m_hitFlash; }
    float spawnPhase() const { return m_phase; }
    float speedScale() const { return m_speedScale; }
    float worldScrollSpeed() const { return m_worldScrollSpeed; }

    void setInvulnerable(float seconds) { m_invulnerable = seconds; }
    void setMinion(bool value) { m_minion = value; }
    void setLifetime(float seconds) { m_lifetime = seconds; }

    // Opaque handle assigned by the scene; TargetInfo round-trips through it.
    std::uint32_t handle() const { return m_handle; }
    void setHandle(std::uint32_t value) { m_handle = value; }

    // --- Death event ------------------------------------------------------
    bool hasDeathEvent() const { return m_hasDeathEvent; }
    const EnemyDeathEvent& deathEvent() const { return m_deathEvent; }
    void clearDeathEvent() { m_hasDeathEvent = false; }

    // --- Spawned children -------------------------------------------------
    // Behaviours (Splitter) push children here; the scene drains the queue
    // after update so GameWorld stays the only owner of enemies.
    void spawnChild(std::unique_ptr<EnemyBase> child);
    bool hasPendingSpawns() const { return !m_pendingSpawns.empty(); }
    std::vector<std::unique_ptr<EnemyBase>> takePendingSpawns();

    IEnemyBehavior* behavior() const { return m_behavior.get(); }

protected:
    // Shared tail of update(): ages timers, integrates, culls. Boss reuses it.
    void integrate(float dt, IGameWorld& world);
    void advanceTimers(float dt);
    void updateCulling(const IGameWorld& world);
    void die(IGameWorld& world, bool killedByPlayer);
    Color renderTint() const;

    EnemyArchetype m_archetype = EnemyArchetype::Drifter;
    std::unique_ptr<IEnemyBehavior> m_behavior;

    Vector2 m_position{ 0.0f, 0.0f };
    Vector2 m_velocity{ 0.0f, 0.0f };
    Vector2 m_size{ 40.0f, 40.0f };
    float m_rotation = 0.0f;
    float m_radius = 18.0f;

    float m_hitPoints = 10.0f;
    float m_maxHitPoints = 10.0f;
    float m_contactDamage = 25.0f;
    int m_scoreValue = 100;

    float m_age = 0.0f;
    float m_hitFlash = 0.0f;
    float m_invulnerable = 0.0f;
    float m_lifetime = 0.0f;

    float m_phase = 0.0f;
    float m_speedScale = 1.0f;
    float m_worldScrollSpeed = 120.0f;

    SpriteId m_sprite = SpriteId::EnemyDrifter;
    Color m_tint{ 1.0f, 1.0f, 1.0f, 1.0f };

    bool m_alive = true;
    bool m_offScreen = false;
    bool m_minion = false;

    bool m_hasDeathEvent = false;
    EnemyDeathEvent m_deathEvent;

    std::uint32_t m_handle = 0;

    std::vector<std::unique_ptr<EnemyBase>> m_pendingSpawns;
};

} // namespace hu
