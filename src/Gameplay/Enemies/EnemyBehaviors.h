#pragma once

// One behaviour struct per archetype.
//
// Each struct owns whatever private state its pattern needs and is plugged into
// an EnemyBase by EnemyFactory. Nothing here knows about the projectile system:
// all enemy fire goes out through IGameWorld::spawnEnemyProjectile via the
// helpers in the `enemyfire` namespace below.

#include "Core/DrawList.h"
#include "Core/GameTypes.h"
#include "Core/Math.h"
#include "Core/SpriteId.h"
#include "Game/Entity.h"
#include "Gameplay/Enemies/EnemyBase.h"

namespace hu {

class IGameWorld;

// ---------------------------------------------------------------------------
// Firing helpers
// ---------------------------------------------------------------------------

namespace enemyfire {

// Description of a single enemy shot. Defaults are the standard light pellet.
struct Shot {
    float speed = 260.0f;
    float damage = 10.0f;
    float radius = 5.0f;
    float lifetime = 6.0f;
    SpriteId sprite = SpriteId::BulletEnemy;
    Vector2 size{ 14.0f, 8.0f };
    Color tint{ 1.0f, 1.0f, 1.0f, 1.0f };
    bool additive = false;
    bool rotateToVelocity = true;
};

bool isBulletHell(const IGameWorld& world);

// Fires one bullet travelling along `direction` (need not be normalised).
void fire(IGameWorld& world, Vector2 origin, Vector2 direction, const Shot& shot);

// Fires one bullet at a fixed angle in radians.
void fireAngle(IGameWorld& world, Vector2 origin, float angle, const Shot& shot);

// Fires with an explicit velocity vector rather than a direction plus the
// shot's speed. Used by patterns that inherit part of the firer's own motion,
// so the bullet stream traces the path the enemy is travelling.
void fireVelocity(IGameWorld& world, Vector2 origin, Vector2 velocity, const Shot& shot);

// Evenly spaced ring of `count` bullets, rotated by `phase` radians.
void fireRing(IGameWorld& world, Vector2 origin, int count, float phase, const Shot& shot);

// `count` bullets fanned symmetrically about `direction` across `spread` radians.
void fireFan(IGameWorld& world, Vector2 origin, Vector2 direction, int count, float spread,
             const Shot& shot);

// Direction from `origin` to the player's current position.
Vector2 aimAtPlayer(const IGameWorld& world, Vector2 origin);

// First-order intercept solution: where to aim so a bullet of `projectileSpeed`
// meets a target moving at `targetVelocity`. Falls back to straight aim when
// no intercept exists.
Vector2 leadTarget(Vector2 origin, Vector2 targetPosition, Vector2 targetVelocity,
                   float projectileSpeed);

// Muzzle flash at the firing point.
void muzzle(IGameWorld& world, Vector2 origin, Vector2 direction, float scale = 0.6f);

} // namespace enemyfire

// ---------------------------------------------------------------------------
// Drifter -- straight-line fodder
// ---------------------------------------------------------------------------

class DrifterBehavior final : public IEnemyBehavior {
public:
    void onSpawn(EnemyBase& self, const EnemySpawnParams& params) override;
    void update(EnemyBase& self, float dt, IGameWorld& world) override;
    const char* name() const override { return "Drifter"; }

private:
    float m_fireTimer = 0.0f;
    float m_bobPhase = 0.0f;
};

// ---------------------------------------------------------------------------
// WaveRider -- sine sweep, spawns in formations
// ---------------------------------------------------------------------------

class WaveRiderBehavior final : public IEnemyBehavior {
public:
    void onSpawn(EnemyBase& self, const EnemySpawnParams& params) override;
    void update(EnemyBase& self, float dt, IGameWorld& world) override;
    const char* name() const override { return "WaveRider"; }

private:
    float m_baseY = 0.0f;
    float m_amplitude = 0.0f;
    float m_wavePhase = 0.0f;
    float m_fireTimer = 0.0f;

    // Bullet-hell curtain state. Successive shots alternate which side of the
    // flight path they are thrown to, and the starting side alternates by
    // formation index, so neighbouring riders weave in opposition.
    int m_formationIndex = 0;
    bool m_curtainFlip = false;
    int m_curtainShot = 0;
};

// ---------------------------------------------------------------------------
// Diver -- approach, dive, strafe past, withdraw, repeat
// ---------------------------------------------------------------------------

class DiverBehavior final : public IEnemyBehavior {
public:
    enum class Stage {
        Approach = 0,  // Slide in from the right to the dive line.
        Wind,          // Brief hover while locking the target (telegraph).
        Dive,          // Commit to the locked point at high speed, firing.
        Strafe,        // Continue past the player.
        Withdraw       // Pull back out to the right for another pass.
    };

    void onSpawn(EnemyBase& self, const EnemySpawnParams& params) override;
    void update(EnemyBase& self, float dt, IGameWorld& world) override;
    void appendDraw(const EnemyBase& self, DrawList& out) const override;
    const char* name() const override { return "Diver"; }

private:
    void enter(Stage stage);

    Stage m_stage = Stage::Approach;
    float m_stageTimer = 0.0f;
    float m_fireTimer = 0.0f;
    int m_burstLeft = 0;
    int m_passCount = 0;
    Vector2 m_lockedTarget{ 0.0f, 0.0f };
    float m_holdY = 0.0f;
};

// ---------------------------------------------------------------------------
// Turret -- world anchored, armoured, leads the player
// ---------------------------------------------------------------------------

class TurretBehavior final : public IEnemyBehavior {
public:
    void onSpawn(EnemyBase& self, const EnemySpawnParams& params) override;
    void update(EnemyBase& self, float dt, IGameWorld& world) override;
    void appendDraw(const EnemyBase& self, DrawList& out) const override;
    const char* name() const override { return "Turret"; }

private:
    float m_fireTimer = 0.0f;
    float m_chargeTimer = 0.0f;   // > 0 while the shot is telegraphed.
    float m_barrelAngle = Pi;
    Vector2 m_pendingAim{ -1.0f, 0.0f };
    // Player velocity is differentiated locally; IGameWorld only exposes the
    // player's position.
    Vector2 m_lastPlayerPos{ 0.0f, 0.0f };
    Vector2 m_playerVelocity{ 0.0f, 0.0f };
    bool m_hasPlayerSample = false;
};

// ---------------------------------------------------------------------------
// Splitter -- lumbering blob that breaks apart on death
// ---------------------------------------------------------------------------

class SplitterBehavior final : public IEnemyBehavior {
public:
    void onSpawn(EnemyBase& self, const EnemySpawnParams& params) override;
    void update(EnemyBase& self, float dt, IGameWorld& world) override;
    void onDeath(EnemyBase& self, IGameWorld& world) override;
    const char* name() const override { return "Splitter"; }

private:
    float m_wobblePhase = 0.0f;
    float m_lobTimer = 0.0f;
};

// ---------------------------------------------------------------------------
// Splitter child -- scattering shard with a short fuse
// ---------------------------------------------------------------------------

class SplitterChildBehavior final : public IEnemyBehavior {
public:
    void onSpawn(EnemyBase& self, const EnemySpawnParams& params) override;
    void update(EnemyBase& self, float dt, IGameWorld& world) override;
    const char* name() const override { return "SplitterChild"; }

private:
    Vector2 m_scatterVelocity{ 0.0f, 0.0f };
    float m_spin = 0.0f;
};

// ---------------------------------------------------------------------------
// Orbiter -- circles a leftward drifting anchor, tracing a cycloid
// ---------------------------------------------------------------------------

class OrbiterBehavior final : public IEnemyBehavior {
public:
    void onSpawn(EnemyBase& self, const EnemySpawnParams& params) override;
    void update(EnemyBase& self, float dt, IGameWorld& world) override;
    void appendDraw(const EnemyBase& self, DrawList& out) const override;
    const char* name() const override { return "Orbiter"; }

private:
    Vector2 m_anchor{ 0.0f, 0.0f };
    float m_angle = 0.0f;
    float m_orbitRadius = 0.0f;
    float m_angularSpeed = 0.0f;
    float m_fireTimer = 0.0f;
};

// ---------------------------------------------------------------------------
// Mine -- inert until provoked, then detonates into a ring
// ---------------------------------------------------------------------------

class MineBehavior final : public IEnemyBehavior {
public:
    void onSpawn(EnemyBase& self, const EnemySpawnParams& params) override;
    void update(EnemyBase& self, float dt, IGameWorld& world) override;
    void onDeath(EnemyBase& self, IGameWorld& world) override;
    void appendDraw(const EnemyBase& self, DrawList& out) const override;
    const char* name() const override { return "Mine"; }

    bool isArming() const { return m_armTimer > 0.0f; }

private:
    float m_bobPhase = 0.0f;
    float m_armTimer = 0.0f;      // Counts down once the player is close.
    float m_pulse = 0.0f;         // Telegraph scale/flash driver.
    bool m_detonated = false;
};

} // namespace hu
