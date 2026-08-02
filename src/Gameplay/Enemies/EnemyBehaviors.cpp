#include "Gameplay/Enemies/EnemyBehaviors.h"

#include "Core/Log.h"
#include "Core/Math.h"
#include "Gameplay/Enemies/EnemyFactory.h"
#include "Gameplay/IGameWorld.h"

#include <cmath>

namespace hu {

// ===========================================================================
// TUNABLES -- every magic number for the seven non-boss archetypes lives here.
// ===========================================================================
namespace {

constexpr const char* kLogCategory = "Enemy";

// --- Shared -----------------------------------------------------------------
// Enemies do not shoot until they are actually on screen; firing from off the
// right edge is invisible and feels unfair.
constexpr float kOnScreenFireMargin = 24.0f;
// Sprites bank by at most this much when matching their heading.
constexpr float kMaxBankRadians = 0.65f;

// --- Drifter ----------------------------------------------------------------
constexpr float kDrifterSpeed = 120.0f;
constexpr float kDrifterBobAmplitude = 14.0f;
constexpr float kDrifterBobRate = 1.7f;
constexpr float kDrifterFireInterval = 2.4f;
constexpr float kDrifterFireIntervalHell = 1.1f;
constexpr float kDrifterBulletSpeed = 250.0f;
constexpr float kDrifterBulletSpeedHell = 340.0f;
constexpr float kDrifterBulletDamage = 8.0f;
constexpr int kDrifterHellFanCount = 3;
constexpr float kDrifterHellFanSpread = 0.42f;

// Bullet hell: the slow wall.
//
// Drifters are the fodder, so they should not be the thing that kills you --
// they should be the thing that shapes where you can stand. A wide arc of slow
// bullets fired infrequently is a drifting wall to weave through rather than a
// threat to react to, and it gives the fast patterns something to push against.
constexpr int kDrifterWallCount = 9;
constexpr float kDrifterWallSpread = 1.45f;
constexpr float kDrifterWallSpeed = 132.0f;
constexpr float kDrifterWallInterval = 2.0f;
constexpr float kDrifterWallDamage = 7.0f;
constexpr float kDrifterWallLifetime = 7.0f;

// --- WaveRider --------------------------------------------------------------
constexpr float kWaveRiderSpeed = 190.0f;
constexpr float kWaveRiderAmplitude = 96.0f;
constexpr float kWaveRiderFrequency = 2.1f;
// Formation members share the cadence but are offset along the wave so the
// squadron snakes in sequence rather than moving as a rigid block.
constexpr float kWaveRiderFormationPhaseStep = 0.55f;
constexpr float kWaveRiderFireInterval = 1.6f;
constexpr float kWaveRiderFireIntervalHell = 0.7f;
constexpr float kWaveRiderBulletSpeed = 300.0f;
constexpr float kWaveRiderBulletSpeedHell = 400.0f;
constexpr float kWaveRiderBulletDamage = 9.0f;
constexpr int kWaveRiderHellFanCount = 3;
constexpr float kWaveRiderHellFanSpread = 0.55f;

// Bullet hell: braided sine curtain.
//
// Each rider fires along both normals of its own wave tangent, so the emission
// direction sweeps as the rider climbs and dives. Because a formation shares
// one wave with a per-member phase offset, the riders are at different points
// of the curve at any instant and their two bullet families cross into a braid.
//
// The normals are mostly vertical (the riders travel left), which would leave
// the bullets drifting up and down out of the player's way, so each is biased
// leftward. That turns the curtain into something that advances on the player
// while keeping the sine structure that makes it readable.
constexpr float kWaveRiderCurtainInterval = 0.07f;
constexpr float kWaveRiderCurtainDamage = 7.0f;
constexpr float kWaveRiderCurtainLifetime = 5.0f;

// Fraction of the rider's own velocity each bullet keeps. This is what makes
// the stream trace the sine path instead of spraying: the bullet leaves along
// the rider's heading and the heading is the wave. It also carries the bullet
// leftward toward the player without an artificial bias.
constexpr float kWaveRiderCurtainInherit = 0.82f;

// Sideways kick away from the flight path, alternating each shot so a single
// rider lays a zigzag ribbon rather than a straight line.
constexpr float kWaveRiderCurtainKick = 165.0f;

// Every few shots the rider throws a wider pair to both sides at once, which
// punctuates the ribbon and gives the formation a visible rhythm.
constexpr int kWaveRiderCurtainFlourishEvery = 6;
constexpr float kWaveRiderCurtainFlourishKick = 260.0f;

// --- Diver ------------------------------------------------------------------
constexpr float kDiverApproachSpeed = 210.0f;
constexpr float kDiverDiveSpeed = 620.0f;
constexpr float kDiverDiveSpeedHell = 760.0f;
constexpr float kDiverWithdrawSpeed = 430.0f;
constexpr float kDiverDiveLineX = 1060.0f;   // Where the approach ends.
constexpr float kDiverWindDuration = 0.45f;
constexpr float kDiverDiveDuration = 0.95f;
constexpr float kDiverStrafeDuration = 0.5f;
constexpr float kDiverWithdrawDuration = 1.5f;
constexpr float kDiverBurstShots = 3;
constexpr float kDiverBurstShotsHell = 5;
constexpr float kDiverBurstInterval = 0.13f;
constexpr float kDiverBulletSpeed = 380.0f;
constexpr float kDiverBulletSpeedHell = 470.0f;
constexpr float kDiverBulletDamage = 10.0f;
constexpr float kDiverReturnX = 1240.0f;
constexpr float kDiverWindGlowSize = 26.0f;

// Bullet hell: the spiral wake.
//
// The dive itself stays ballistic and sidesteppable -- that is what makes it
// fair. What changes is that the diver sheds a curling trail along its path, so
// the sidestep is only half the problem: the lane the diver came through stays
// dangerous behind it. Slow bullets on a rotating emission angle leave a
// corkscrew hanging in the air rather than a straight line.
constexpr float kDiverWakeInterval = 0.045f;
constexpr float kDiverWakeSpeed = 120.0f;
constexpr float kDiverWakeDamage = 7.0f;
constexpr float kDiverWakeSpinRate = 7.5f;    // radians/second
constexpr int kDiverWakeArms = 2;
constexpr float kDiverWakeLifetime = 4.0f;

// --- Turret -----------------------------------------------------------------
constexpr float kTurretFireInterval = 2.1f;
constexpr float kTurretFireIntervalHell = 1.0f;
constexpr float kTurretChargeDuration = 0.55f;   // Telegraph before the shot.
constexpr float kTurretBulletSpeed = 420.0f;
constexpr float kTurretBulletSpeedHell = 520.0f;
constexpr float kTurretBulletDamage = 16.0f;
constexpr float kTurretBarrelTurnRate = 2.4f;
// Exponential smoothing rate for the turret's estimate of player velocity.
constexpr float kTurretAimSmoothing = 8.0f;
constexpr float kTurretBarrelLength = 26.0f;
constexpr float kTurretBarrelWidth = 9.0f;
constexpr int kTurretHellSalvo = 3;
constexpr float kTurretHellSalvoSpread = 0.24f;

// Bullet hell: lighthouse sweep.
//
// Turrets are bolted to the scrolling world, so a steadily rotating emitter
// becomes a fixed hazard the player scrolls into rather than one that chases
// them. The full rotation matters: the half-turn pointing away is the breathing
// room that makes the half pointing at you survivable. This runs alongside the
// existing telegraphed aimed shot, which stays the thing that punishes standing
// still in a safe wedge.
constexpr float kTurretSweepRate = 1.5f;         // radians/second
constexpr float kTurretSweepInterval = 0.075f;
constexpr float kTurretSweepBulletSpeed = 215.0f;
constexpr float kTurretSweepDamage = 8.0f;
constexpr int kTurretSweepArms = 2;              // Opposed arms: a bar, not a spoke.

// --- Splitter ---------------------------------------------------------------
constexpr float kSplitterSpeed = 78.0f;
constexpr float kSplitterWobbleAmplitude = 26.0f;
constexpr float kSplitterWobbleRate = 0.9f;
constexpr int kSplitterChildCountMin = 2;
constexpr int kSplitterChildCountMax = 3;
constexpr int kSplitterChildCountHellBonus = 1;
constexpr float kSplitterChildScatterSpeed = 210.0f;
constexpr float kSplitterChildLifetime = 3.4f;
constexpr float kSplitterLobInterval = 3.0f;
constexpr float kSplitterLobIntervalHell = 1.5f;
constexpr float kSplitterLobSpeed = 190.0f;
constexpr float kSplitterLobDamage = 12.0f;
constexpr int kSplitterLobRingCount = 6;

// Bullet hell: cascading rings.
//
// The parent lays a ring as it bursts, and every child lays a smaller one when
// it dies in turn, so clearing a splitter cluster sets off a chain rather than
// a single event. Rings are rotated off each other so the layers do not
// overlap into a solid wall.
constexpr int kSplitterDeathRingCount = 14;
constexpr float kSplitterDeathRingSpeed = 205.0f;
constexpr int kSplitterChildDeathRingCount = 8;
constexpr float kSplitterChildDeathRingSpeed = 175.0f;
constexpr float kSplitterCascadeDamage = 10.0f;

// --- Splitter child ---------------------------------------------------------
constexpr float kChildDrag = 0.9f;            // Per second, multiplicative.
constexpr float kChildSpinRate = 7.5f;
constexpr float kChildBaseDrift = -90.0f;

// --- Orbiter ----------------------------------------------------------------
constexpr float kOrbiterAnchorSpeed = 105.0f;
constexpr float kOrbiterRadius = 84.0f;
constexpr float kOrbiterAngularSpeed = 2.2f;
constexpr float kOrbiterFireInterval = 1.35f;
constexpr float kOrbiterFireIntervalHell = 0.55f;
constexpr float kOrbiterBulletSpeed = 280.0f;
constexpr float kOrbiterBulletSpeedHell = 360.0f;
constexpr float kOrbiterBulletDamage = 9.0f;
constexpr float kOrbiterTetherAlpha = 0.18f;
constexpr float kOrbiterTetherSize = 6.0f;

// Bullet hell: spiral arms.
//
// Firing radially outward from a point that is itself orbiting traces a spiral,
// because the emission angle advances with the orbit. Several evenly-spaced
// arms fired together give the classic rotating pinwheel, and because orbiters
// counter-rotate on odd formation indices, neighbouring ones lay opposing
// spirals that cross into an interference lattice.
//
// The cadence has to be short or the arm reads as separate bullets instead of a
// continuous curve, and the bullets have to be slow or the spiral unwinds off
// screen before its shape is legible.
constexpr float kOrbiterSpiralInterval = 0.055f;
constexpr int kOrbiterSpiralArms = 3;
constexpr float kOrbiterSpiralBulletSpeed = 190.0f;
constexpr float kOrbiterSpiralDamage = 7.0f;
constexpr float kOrbiterSpiralLifetime = 4.5f;

// --- Mine -------------------------------------------------------------------
constexpr float kMineDrift = -62.0f;
constexpr float kMineBobAmplitude = 9.0f;
constexpr float kMineBobRate = 1.1f;
constexpr float kMineProximityRadius = 132.0f;
constexpr float kMineArmDuration = 0.75f;       // Telegraph window.
constexpr float kMineArmPulseRate = 22.0f;
constexpr float kMineRingCount = 12;
constexpr float kMineRingCountHell = 22;
constexpr float kMineRingSpeed = 240.0f;
constexpr float kMineRingSpeedHell = 300.0f;
constexpr float kMineRingDamage = 12.0f;
constexpr float kMineGlowScale = 2.1f;

// Bullet hell: double detonation.
//
// Two concentric rings released together but travelling at different speeds,
// the inner one offset by half a step so it threads the gaps of the outer as it
// overtakes. Reading the first ring's gaps is not enough -- the second one
// closes them, so the player has to move rather than sit in a slot. Expressing
// it as a speed difference rather than a delayed second blast means the mine
// does not need to outlive its own death.
constexpr int kMineOuterRingCount = 20;
constexpr float kMineOuterRingSpeed = 190.0f;
constexpr int kMineInnerRingCount = 20;
constexpr float kMineInnerRingSpeed = 330.0f;

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

bool onScreenToFire(const EnemyBase& self, const IGameWorld& world) {
    return self.position().x < world.screenWidth() - kOnScreenFireMargin &&
           self.position().x > -kOnScreenFireMargin;
}

// Picks the value that matches the current difficulty. Keeps the archetype's
// identity intact -- same behaviour, angrier numbers.
float byMode(const IGameWorld& world, float normal, float hell) {
    return world.difficulty() == DifficultyMode::BulletHell ? hell : normal;
}

// Banks a sprite toward its heading without letting it flip end over end.
void bankToHeading(EnemyBase& self, float maxBank = kMaxBankRadians) {
    const Vector2 v = self.velocity();
    if (lengthSquared(v) < 1.0f) {
        self.setRotation(0.0f);
        return;
    }
    const float speed = length(v);
    self.setRotation(clampf(v.y / speed, -1.0f, 1.0f) * maxBank);
}

} // namespace

// ===========================================================================
// enemyfire helpers
// ===========================================================================

namespace enemyfire {

bool isBulletHell(const IGameWorld& world) {
    return world.difficulty() == DifficultyMode::BulletHell;
}

void fire(IGameWorld& world, Vector2 origin, Vector2 direction, const Shot& shot) {
    const Vector2 dir = normalize(direction);
    if (lengthSquared(dir) < 0.5f) {
        return;
    }
    ProjectileSpawn spawn;
    spawn.position = origin;
    spawn.velocity = scale(dir, shot.speed);
    spawn.damage = shot.damage;
    spawn.radius = shot.radius;
    spawn.lifetime = shot.lifetime;
    spawn.motion = ProjectileMotion::Straight;
    spawn.sprite = shot.sprite;
    spawn.size = shot.size;
    spawn.tint = shot.tint;
    spawn.additive = shot.additive;
    spawn.maxSpeed = shot.speed;
    world.spawnEnemyProjectile(spawn);
}

void fireAngle(IGameWorld& world, Vector2 origin, float angle, const Shot& shot) {
    fire(world, origin, fromAngle(angle), shot);
}

void fireVelocity(IGameWorld& world, Vector2 origin, Vector2 velocity, const Shot& shot) {
    const float speed = length(velocity);
    if (speed < 1.0f) {
        return;
    }
    ProjectileSpawn spawn;
    spawn.position = origin;
    spawn.velocity = velocity;
    spawn.damage = shot.damage;
    spawn.radius = shot.radius;
    spawn.lifetime = shot.lifetime;
    spawn.motion = ProjectileMotion::Straight;
    spawn.sprite = shot.sprite;
    spawn.size = shot.size;
    spawn.tint = shot.tint;
    spawn.additive = shot.additive;
    spawn.maxSpeed = speed;
    world.spawnEnemyProjectile(spawn);
}

void fireRing(IGameWorld& world, Vector2 origin, int count, float phase, const Shot& shot) {
    if (count <= 0) {
        return;
    }
    const float step = TwoPi / static_cast<float>(count);
    for (int i = 0; i < count; ++i) {
        fireAngle(world, origin, phase + step * static_cast<float>(i), shot);
    }
}

void fireFan(IGameWorld& world, Vector2 origin, Vector2 direction, int count, float spread,
             const Shot& shot) {
    if (count <= 0) {
        return;
    }
    if (count == 1) {
        fire(world, origin, direction, shot);
        return;
    }
    const float base = angleOf(normalize(direction));
    const float step = spread / static_cast<float>(count - 1);
    const float start = base - spread * 0.5f;
    for (int i = 0; i < count; ++i) {
        fireAngle(world, origin, start + step * static_cast<float>(i), shot);
    }
}

Vector2 aimAtPlayer(const IGameWorld& world, Vector2 origin) {
    return normalize(sub(world.playerPosition(), origin));
}

Vector2 leadTarget(Vector2 origin, Vector2 targetPosition, Vector2 targetVelocity,
                   float projectileSpeed) {
    const Vector2 toTarget = sub(targetPosition, origin);
    if (projectileSpeed <= 1.0f) {
        return normalize(toTarget);
    }

    // Solve |toTarget + targetVelocity * t| = projectileSpeed * t for t.
    const float a = lengthSquared(targetVelocity) - projectileSpeed * projectileSpeed;
    const float b = 2.0f * dot(toTarget, targetVelocity);
    const float c = lengthSquared(toTarget);

    float t = -1.0f;
    if (std::fabs(a) < 1e-3f) {
        if (std::fabs(b) > 1e-3f) {
            t = -c / b;
        }
    } else {
        const float disc = b * b - 4.0f * a * c;
        if (disc >= 0.0f) {
            const float root = std::sqrt(disc);
            const float t0 = (-b - root) / (2.0f * a);
            const float t1 = (-b + root) / (2.0f * a);
            // Prefer the earliest positive intercept.
            if (t0 > 0.0f && t1 > 0.0f) {
                t = t0 < t1 ? t0 : t1;
            } else {
                t = t0 > 0.0f ? t0 : t1;
            }
        }
    }

    if (t <= 0.0f) {
        return normalize(toTarget);
    }
    return normalize(add(toTarget, scale(targetVelocity, t)));
}

void muzzle(IGameWorld& world, Vector2 origin, Vector2 direction, float scale) {
    EffectRequest fx;
    fx.kind = EffectKind::MuzzleFlash;
    fx.position = origin;
    fx.direction = normalize(direction);
    fx.scale = scale;
    // The flash is shared with the player's weapons, but the sound must not be:
    // the player needs to hear incoming fire as distinct from their own, which
    // matters most in bullet hell where the screen is full of both.
    fx.silent = true;
    world.spawnEffect(fx);
    world.playSoundAt(SoundId::EnemyFire, origin, 0.45f);
}

} // namespace enemyfire

// ===========================================================================
// Drifter
//   Movement: constant leftward drift with a lazy vertical bob.
//   Weapon:   one aimed pellet on a long timer (a short fan in Bullet Hell).
// ===========================================================================

void DrifterBehavior::onSpawn(EnemyBase& self, const EnemySpawnParams& params) {
    m_bobPhase = params.phase;
    // Stagger the first shot so a squad of drifters does not volley in unison.
    m_fireTimer = kDrifterFireInterval * 0.5f + randomRange(0.0f, 0.8f);
    self.setVelocity({ -kDrifterSpeed * params.speedScale, 0.0f });
}

void DrifterBehavior::update(EnemyBase& self, float dt, IGameWorld& world) {
    m_bobPhase += dt * kDrifterBobRate;

    const float vx = -kDrifterSpeed * self.speedScale();
    const float vy = std::cos(m_bobPhase) * kDrifterBobAmplitude * kDrifterBobRate;
    self.setVelocity({ vx, vy });
    bankToHeading(self, 0.25f);

    m_fireTimer -= dt;
    if (m_fireTimer <= 0.0f && onScreenToFire(self, world)) {
        const Vector2 dir = enemyfire::aimAtPlayer(world, self.position());

        if (enemyfire::isBulletHell(world)) {
            // A wide, slow wall rather than a faster aimed fan.
            m_fireTimer = kDrifterWallInterval;

            enemyfire::Shot shot;
            shot.speed = kDrifterWallSpeed;
            shot.damage = kDrifterWallDamage;
            shot.lifetime = kDrifterWallLifetime;
            enemyfire::fireFan(world, self.position(), dir, kDrifterWallCount,
                               kDrifterWallSpread, shot);
        } else {
            m_fireTimer = kDrifterFireInterval;

            enemyfire::Shot shot;
            shot.speed = kDrifterBulletSpeed;
            shot.damage = kDrifterBulletDamage;
            enemyfire::fire(world, self.position(), dir, shot);
        }
        enemyfire::muzzle(world, self.position(), dir);
    }
}

// ===========================================================================
// WaveRider
//   Movement: sine sweep across a base line; formation members are offset along
//             the same wave so five of them snake through in sequence.
//   Weapon:   straight shots on a cadence shared by the whole formation.
// ===========================================================================

void WaveRiderBehavior::onSpawn(EnemyBase& self, const EnemySpawnParams& params) {
    m_baseY = params.position.y;
    m_amplitude = params.amplitude > 0.0f ? params.amplitude : kWaveRiderAmplitude;
    m_wavePhase = params.phase +
                  static_cast<float>(params.formationIndex) * kWaveRiderFormationPhaseStep;
    // Cadence is shared: derived from the phase, not randomised per member, so
    // a squadron volleys together.
    m_fireTimer = kWaveRiderFireInterval;
    m_formationIndex = params.formationIndex;
    m_curtainFlip = (params.formationIndex % 2) == 0;
    self.setVelocity({ -kWaveRiderSpeed * params.speedScale, 0.0f });
}

void WaveRiderBehavior::update(EnemyBase& self, float dt, IGameWorld& world) {
    m_wavePhase += dt * kWaveRiderFrequency;

    const float vx = -kWaveRiderSpeed * self.speedScale();
    const float vy = std::cos(m_wavePhase) * m_amplitude * kWaveRiderFrequency;

    // Drive y from the wave directly so accumulated drift cannot flatten it.
    Vector2 pos = self.position();
    pos.y = m_baseY + std::sin(m_wavePhase) * m_amplitude;
    self.setPosition(pos);
    self.setVelocity({ vx, vy });

    // Deliberately not banked. Each rider in a formation sits at a different
    // point in the shared wave, so banking toward the heading left every member
    // of a squadron tilted by a different amount at any instant, which read as
    // broken rather than as motion. They stay level.
    self.setRotation(0.0f);

    m_fireTimer -= dt;
    if (m_fireTimer <= 0.0f && onScreenToFire(self, world)) {
        if (enemyfire::isBulletHell(world)) {
            // Braided ribbon.
            //
            // Each bullet keeps most of the rider's velocity, so it leaves
            // along the wave and the stream draws the sine curve in the air.
            // A sideways kick that flips every shot turns that line into a
            // zigzag, and because the starting side alternates by formation
            // index while the members sit at different wave phases, the
            // squadron's ribbons cross into a lattice.
            m_fireTimer = kWaveRiderCurtainInterval;

            enemyfire::Shot shot;
            shot.damage = kWaveRiderCurtainDamage;
            shot.lifetime = kWaveRiderCurtainLifetime;

            const Vector2 velocity{ vx, vy };
            const Vector2 tangent = normalize(velocity);
            const Vector2 normal{ -tangent.y, tangent.x };
            const Vector2 inherited = scale(velocity, kWaveRiderCurtainInherit);

            m_curtainFlip = !m_curtainFlip;
            const float side = m_curtainFlip ? 1.0f : -1.0f;

            enemyfire::fireVelocity(world, self.position(),
                                    add(inherited, scale(normal, kWaveRiderCurtainKick * side)),
                                    shot);

            // Periodic flourish: a wide pair to both sides, giving the ribbon
            // punctuation instead of a uniform stream.
            ++m_curtainShot;
            if (m_curtainShot % kWaveRiderCurtainFlourishEvery == 0) {
                enemyfire::fireVelocity(
                    world, self.position(),
                    add(inherited, scale(normal, kWaveRiderCurtainFlourishKick)), shot);
                enemyfire::fireVelocity(
                    world, self.position(),
                    add(inherited, scale(normal, -kWaveRiderCurtainFlourishKick)), shot);
            }
        } else {
            m_fireTimer = kWaveRiderFireInterval;

            enemyfire::Shot shot;
            shot.speed = kWaveRiderBulletSpeed;
            shot.damage = kWaveRiderBulletDamage;

            // WaveRiders shoot straight ahead, not aimed -- dodging the wave is
            // the puzzle, and aimed fire would make the formation unfair.
            enemyfire::fire(world, self.position(), Vector2{ -1.0f, 0.0f }, shot);
        }
    }
}

// ===========================================================================
// Diver
//   Movement: slides in, hovers to lock (telegraph), dives hard at the player's
//             position at lock time, strafes past, pulls back out right, repeats.
//   Weapon:   a tight burst fired only during the dive.
// ===========================================================================

void DiverBehavior::onSpawn(EnemyBase& self, const EnemySpawnParams& params) {
    m_holdY = params.position.y;
    m_stage = Stage::Approach;
    m_stageTimer = 0.0f;
    self.setVelocity({ -kDiverApproachSpeed * params.speedScale, 0.0f });
}

void DiverBehavior::enter(Stage stage) {
    m_stage = stage;
    m_stageTimer = 0.0f;
}

void DiverBehavior::update(EnemyBase& self, float dt, IGameWorld& world) {
    m_stageTimer += dt;

    switch (m_stage) {
        case Stage::Approach: {
            self.setVelocity({ -kDiverApproachSpeed * self.speedScale(), 0.0f });
            if (self.position().x <= kDiverDiveLineX) {
                enter(Stage::Wind);
            }
            break;
        }
        case Stage::Wind: {
            // Nearly stationary: this is the player's cue that a dive is coming.
            self.setVelocity({ -18.0f, 0.0f });
            m_lockedTarget = world.playerPosition();
            if (m_stageTimer >= kDiverWindDuration) {
                const float speed = byMode(world, kDiverDiveSpeed, kDiverDiveSpeedHell);
                const Vector2 dir = normalize(sub(m_lockedTarget, self.position()));
                self.setVelocity(scale(dir, speed));
                m_burstLeft = static_cast<int>(
                    byMode(world, kDiverBurstShots, kDiverBurstShotsHell));
                m_fireTimer = 0.0f;
                enter(Stage::Dive);
            }
            break;
        }
        case Stage::Dive: {
            // Velocity was locked on entry; the dive is deliberately ballistic
            // so the player can sidestep it.
            //
            // In bullet hell the diver also sheds a corkscrew wake, so the lane
            // it came through stays dangerous after it has passed.
            if (enemyfire::isBulletHell(world)) {
                m_wakeAngle += kDiverWakeSpinRate * dt;
                m_wakeTimer -= dt;
                if (m_wakeTimer <= 0.0f && onScreenToFire(self, world)) {
                    m_wakeTimer = kDiverWakeInterval;

                    enemyfire::Shot wake;
                    wake.speed = kDiverWakeSpeed;
                    wake.damage = kDiverWakeDamage;
                    wake.lifetime = kDiverWakeLifetime;
                    wake.radius = 5.0f;
                    wake.size = { 12.0f, 12.0f };
                    wake.tint = Color{ 1.0f, 0.45f, 0.45f, 1.0f };
                    wake.additive = true;

                    const float armStep = TwoPi / static_cast<float>(kDiverWakeArms);
                    for (int arm = 0; arm < kDiverWakeArms; ++arm) {
                        enemyfire::fireAngle(world, self.position(),
                                             m_wakeAngle + armStep * static_cast<float>(arm),
                                             wake);
                    }
                }
            }

            m_fireTimer -= dt;
            if (m_burstLeft > 0 && m_fireTimer <= 0.0f && onScreenToFire(self, world)) {
                m_fireTimer = kDiverBurstInterval;
                --m_burstLeft;

                enemyfire::Shot shot;
                shot.speed = byMode(world, kDiverBulletSpeed, kDiverBulletSpeedHell);
                shot.damage = kDiverBulletDamage;
                const Vector2 dir = normalize(self.velocity());
                enemyfire::fire(world, self.position(), dir, shot);
                enemyfire::muzzle(world, self.position(), dir, 0.5f);
            }
            if (m_stageTimer >= kDiverDiveDuration) {
                enter(Stage::Strafe);
            }
            break;
        }
        case Stage::Strafe: {
            if (m_stageTimer >= kDiverStrafeDuration) {
                ++m_passCount;
                enter(Stage::Withdraw);
            }
            break;
        }
        case Stage::Withdraw: {
            // Swing back out to the right, climbing toward a fresh hold line.
            const float targetY = m_holdY;
            const float dy = targetY - self.position().y;
            self.setVelocity({ kDiverWithdrawSpeed * self.speedScale(),
                               clampf(dy * 2.0f, -260.0f, 260.0f) });
            const bool timedOut = m_stageTimer >= kDiverWithdrawDuration;
            if (timedOut || self.position().x >= kDiverReturnX) {
                enter(Stage::Approach);
            }
            break;
        }
    }

    // Divers point where they are going; it sells the commitment of the dive.
    if (lengthSquared(self.velocity()) > 1.0f) {
        const float heading = angleOf(self.velocity());
        // Sprite art faces left, so add Pi to align with the heading.
        self.setRotation(heading + Pi);
    }
}

void DiverBehavior::appendDraw(const EnemyBase& self, DrawList& out) const {
    if (m_stage != Stage::Wind) {
        return;
    }
    // Telegraph: a pulsing glow while the diver locks on.
    const float t = clampf(m_stageTimer / kDiverWindDuration, 0.0f, 1.0f);
    const float s = kDiverWindGlowSize * (0.6f + t);
    Color glow{ 1.0f, 0.45f, 0.25f, 0.35f + 0.4f * t };
    out.add(SpriteId::ParticleGlow, self.position(), { s, s }, DrawLayer::EnemyTrail, glow,
            0.0f, true);
}

// ===========================================================================
// Turret
//   Movement: none of its own -- it rides the scroll at exactly world speed, so
//             it reads as bolted to the terrain.
//   Weapon:   slow, heavy, charge-telegraphed shot that LEADS the player.
// ===========================================================================

void TurretBehavior::onSpawn(EnemyBase& self, const EnemySpawnParams& params) {
    m_fireTimer = kTurretFireInterval * 0.6f + params.phase;
    // Alternate sweep direction down a row of turrets so their arms scissor
    // past each other instead of rotating in lockstep, and stagger the starting
    // angle so the gaps do not all line up.
    m_sweepDirection = (params.formationIndex % 2) == 0 ? 1.0f : -1.0f;
    m_sweepAngle = params.phase + static_cast<float>(params.formationIndex) * 0.9f;
    self.setVelocity({ -params.worldScrollSpeed, 0.0f });
}

void TurretBehavior::update(EnemyBase& self, float dt, IGameWorld& world) {
    // Exactly scroll speed: no speedScale, or it would slide against the world.
    self.setVelocity({ -self.worldScrollSpeed(), 0.0f });
    self.setRotation(0.0f);

    const Vector2 playerPos = world.playerPosition();
    const float bulletSpeed = byMode(world, kTurretBulletSpeed, kTurretBulletSpeedHell);

    // IGameWorld exposes the player's position but not its velocity, so the
    // turret differentiates it itself and smooths the result -- a raw
    // frame-to-frame delta is far too jittery to lead with.
    if (dt > 1e-5f) {
        if (m_hasPlayerSample) {
            const Vector2 instant = scale(sub(playerPos, m_lastPlayerPos), 1.0f / dt);
            m_playerVelocity = lerp(m_playerVelocity, instant,
                                    clampf(dt * kTurretAimSmoothing, 0.0f, 1.0f));
        }
        m_lastPlayerPos = playerPos;
        m_hasPlayerSample = true;
    }

    const Vector2 lead = enemyfire::leadTarget(self.position(), playerPos, m_playerVelocity,
                                               bulletSpeed);
    m_barrelAngle = rotateTowards(m_barrelAngle, angleOf(lead), kTurretBarrelTurnRate * dt);

    // Lighthouse sweep runs on its own clock, independent of the aimed shot, so
    // it keeps turning through the telegraph and the recovery.
    if (enemyfire::isBulletHell(world)) {
        m_sweepAngle += kTurretSweepRate * m_sweepDirection * dt;

        m_sweepTimer -= dt;
        if (m_sweepTimer <= 0.0f && onScreenToFire(self, world)) {
            m_sweepTimer = kTurretSweepInterval;

            enemyfire::Shot sweep;
            sweep.speed = kTurretSweepBulletSpeed;
            sweep.damage = kTurretSweepDamage;
            sweep.radius = 5.0f;
            sweep.size = { 13.0f, 9.0f };
            sweep.tint = Color{ 1.0f, 0.75f, 0.45f, 1.0f };

            const float armStep = TwoPi / static_cast<float>(kTurretSweepArms);
            for (int arm = 0; arm < kTurretSweepArms; ++arm) {
                enemyfire::fireAngle(world, self.position(),
                                     m_sweepAngle + armStep * static_cast<float>(arm), sweep);
            }
        }
    }

    if (m_chargeTimer > 0.0f) {
        m_chargeTimer -= dt;
        if (m_chargeTimer <= 0.0f && onScreenToFire(self, world)) {
            enemyfire::Shot shot;
            shot.speed = bulletSpeed;
            shot.damage = kTurretBulletDamage;
            shot.sprite = SpriteId::BulletEnemyHeavy;
            shot.radius = 8.0f;
            shot.size = { 20.0f, 12.0f };

            const Vector2 muzzlePos = add(self.position(),
                                          fromAngle(m_barrelAngle, kTurretBarrelLength));
            const Vector2 dir = fromAngle(m_barrelAngle);
            if (enemyfire::isBulletHell(world)) {
                enemyfire::fireFan(world, muzzlePos, dir, kTurretHellSalvo,
                                   kTurretHellSalvoSpread, shot);
            } else {
                enemyfire::fire(world, muzzlePos, dir, shot);
            }
            enemyfire::muzzle(world, muzzlePos, dir, 0.9f);
        }
        return;
    }

    m_fireTimer -= dt;
    if (m_fireTimer <= 0.0f && onScreenToFire(self, world)) {
        m_fireTimer = byMode(world, kTurretFireInterval, kTurretFireIntervalHell);
        m_chargeTimer = kTurretChargeDuration;
        m_pendingAim = lead;
    }
}

void TurretBehavior::appendDraw(const EnemyBase& self, DrawList& out) const {
    // Barrel: a stubby quad offset along the aim so the player can read the
    // turret's lead before it fires.
    const Vector2 barrelPos = add(self.position(),
                                  fromAngle(m_barrelAngle, kTurretBarrelLength * 0.5f));
    Color barrel{ 0.72f, 0.76f, 0.82f, 1.0f };
    out.add(SpriteId::White, barrelPos, { kTurretBarrelLength, kTurretBarrelWidth },
            DrawLayer::EnemyTrail, barrel, m_barrelAngle);

    if (m_chargeTimer > 0.0f) {
        const float t = 1.0f - clampf(m_chargeTimer / kTurretChargeDuration, 0.0f, 1.0f);
        const Vector2 muzzlePos = add(self.position(),
                                      fromAngle(m_barrelAngle, kTurretBarrelLength));
        const float s = 10.0f + 16.0f * t;
        Color glow{ 1.0f, 0.85f, 0.3f, 0.35f + 0.5f * t };
        out.add(SpriteId::ParticleGlow, muzzlePos, { s, s }, DrawLayer::Particle, glow, 0.0f,
                true);
    }
}

// ===========================================================================
// Splitter
//   Movement: slow lumbering drift with a heavy vertical wobble.
//   Weapon:   occasional slow lobbed cluster; the real threat is what it leaves
//             behind when it dies.
// ===========================================================================

void SplitterBehavior::onSpawn(EnemyBase& self, const EnemySpawnParams& params) {
    m_wobblePhase = params.phase;
    m_lobTimer = kSplitterLobInterval * 0.7f;
    self.setVelocity({ -kSplitterSpeed * params.speedScale, 0.0f });
}

void SplitterBehavior::update(EnemyBase& self, float dt, IGameWorld& world) {
    m_wobblePhase += dt * kSplitterWobbleRate;
    self.setVelocity({ -kSplitterSpeed * self.speedScale(),
                       std::cos(m_wobblePhase) * kSplitterWobbleAmplitude * kSplitterWobbleRate });
    // The blob squashes rather than banks.
    self.setRotation(std::sin(m_wobblePhase * 0.5f) * 0.12f);

    m_lobTimer -= dt;
    if (m_lobTimer <= 0.0f && onScreenToFire(self, world)) {
        m_lobTimer = byMode(world, kSplitterLobInterval, kSplitterLobIntervalHell);

        enemyfire::Shot shot;
        shot.speed = kSplitterLobSpeed;
        shot.damage = kSplitterLobDamage;
        shot.sprite = SpriteId::BulletEnemyHeavy;
        shot.radius = 7.0f;
        shot.size = { 16.0f, 16.0f };

        if (enemyfire::isBulletHell(world)) {
            enemyfire::fireRing(world, self.position(), kSplitterLobRingCount, m_wobblePhase,
                                shot);
        } else {
            enemyfire::fireFan(world, self.position(), { -1.0f, 0.0f }, 2, 0.7f, shot);
        }
    }
}

void SplitterBehavior::onDeath(EnemyBase& self, IGameWorld& world) {
    int count = kSplitterChildCountMin +
                static_cast<int>(randomRange(0.0f,
                                             static_cast<float>(kSplitterChildCountMax -
                                                                kSplitterChildCountMin + 1)));
    if (count > kSplitterChildCountMax) {
        count = kSplitterChildCountMax;
    }
    if (enemyfire::isBulletHell(world)) {
        count += kSplitterChildCountHellBonus;
    }

    const float step = TwoPi / static_cast<float>(count);
    const float base = randomRange(0.0f, TwoPi);
    for (int i = 0; i < count; ++i) {
        const float angle = base + step * static_cast<float>(i);
        EnemySpawnParams params;
        params.position = add(self.position(), fromAngle(angle, self.radius() * 0.5f));
        params.initialVelocity = fromAngle(angle, kSplitterChildScatterSpeed);
        params.phase = angle;
        params.lifetime = kSplitterChildLifetime;
        params.worldScrollSpeed = self.worldScrollSpeed();
        self.spawnChild(EnemyFactory::createSplitterChild(params));
    }

    // Cascade: the burst itself lays a ring, and each child will lay another
    // when it dies, so clearing a cluster chains instead of resolving at once.
    if (enemyfire::isBulletHell(world)) {
        enemyfire::Shot ring;
        ring.speed = kSplitterDeathRingSpeed;
        ring.damage = kSplitterCascadeDamage;
        ring.radius = 6.0f;
        ring.size = { 14.0f, 14.0f };
        ring.tint = Color{ 0.55f, 1.0f, 0.6f, 1.0f };
        ring.additive = true;
        enemyfire::fireRing(world, self.position(), kSplitterDeathRingCount,
                            base + step * 0.5f, ring);
    }

    HU_LOG_DEBUG(kLogCategory, "Splitter burst into %d children at (%.0f, %.0f)", count,
                 self.position().x, self.position().y);
    world.addScreenShake(0.25f, 0.18f);
}

// ===========================================================================
// Splitter child
//   Movement: scatters outward from the parent, drag bleeds it back into the
//             scroll; spins the whole time. Never shoots -- it is a contact
//             hazard on a short fuse.
// ===========================================================================

void SplitterChildBehavior::onSpawn(EnemyBase& self, const EnemySpawnParams& params) {
    m_scatterVelocity = params.initialVelocity;
    m_spin = params.phase;
    self.setVelocity(m_scatterVelocity);
}

void SplitterChildBehavior::onDeath(EnemyBase& self, IGameWorld& world) {
    if (!enemyfire::isBulletHell(world)) {
        return;
    }

    enemyfire::Shot ring;
    ring.speed = kSplitterChildDeathRingSpeed;
    ring.damage = kSplitterCascadeDamage;
    ring.radius = 5.0f;
    ring.size = { 12.0f, 12.0f };
    ring.tint = Color{ 0.7f, 1.0f, 0.75f, 1.0f };
    ring.additive = true;

    enemyfire::fireRing(world, self.position(), kSplitterChildDeathRingCount,
                        randomRange(0.0f, TwoPi), ring);
}

void SplitterChildBehavior::update(EnemyBase& self, float dt, IGameWorld&) {
    // Exponential drag toward the ambient leftward drift.
    const float decay = std::pow(kChildDrag, dt * 10.0f);
    m_scatterVelocity = scale(m_scatterVelocity, decay);
    self.setVelocity({ m_scatterVelocity.x + kChildBaseDrift, m_scatterVelocity.y });

    m_spin += dt * kChildSpinRate;
    self.setRotation(m_spin);
}

// ===========================================================================
// Orbiter
//   Movement: circles an invisible anchor that itself drifts left, so the path
//             traced through the world is a cycloid.
//   Weapon:   fires tangentially to the orbit -- the bullets spray off the
//             circle like sparks off a wheel.
// ===========================================================================

void OrbiterBehavior::onSpawn(EnemyBase& self, const EnemySpawnParams& params) {
    m_orbitRadius = params.amplitude > 0.0f ? params.amplitude : kOrbiterRadius;
    m_angle = params.phase;
    // Alternate spin direction by formation index so a pair counter-rotates.
    m_angularSpeed = kOrbiterAngularSpeed * ((params.formationIndex % 2 == 0) ? 1.0f : -1.0f);
    m_anchor = params.position;
    self.setPosition(add(m_anchor, fromAngle(m_angle, m_orbitRadius)));
    m_fireTimer = kOrbiterFireInterval * 0.5f;
}

void OrbiterBehavior::update(EnemyBase& self, float dt, IGameWorld& world) {
    const Vector2 previous = self.position();

    m_anchor.x -= kOrbiterAnchorSpeed * self.speedScale() * dt;
    m_angle += m_angularSpeed * dt;

    const Vector2 next = add(m_anchor, fromAngle(m_angle, m_orbitRadius));
    self.setPosition(next);
    // Derive velocity so targeting/leading code sees the true motion, then zero
    // the integration step by letting the base add it back to the same point.
    if (dt > 1e-5f) {
        self.setVelocity(scale(sub(next, previous), 1.0f / dt));
    }
    // Base::integrate() will advance by velocity*dt again, so pre-compensate.
    self.setPosition(previous);

    self.setRotation(m_angle + Pi * 0.5f);

    m_fireTimer -= dt;
    if (m_fireTimer <= 0.0f && onScreenToFire(self, world)) {
        if (enemyfire::isBulletHell(world)) {
            // Spiral arms: emit radially outward on a fast cadence. The orbit
            // advances the emission angle for us, so successive shots lay a
            // curve rather than a line.
            m_fireTimer = kOrbiterSpiralInterval;

            enemyfire::Shot shot;
            shot.speed = kOrbiterSpiralBulletSpeed;
            shot.damage = kOrbiterSpiralDamage;
            shot.lifetime = kOrbiterSpiralLifetime;

            const float armStep = TwoPi / static_cast<float>(kOrbiterSpiralArms);
            for (int arm = 0; arm < kOrbiterSpiralArms; ++arm) {
                enemyfire::fireAngle(world, next,
                                     m_angle + armStep * static_cast<float>(arm), shot);
            }
        } else {
            m_fireTimer = kOrbiterFireInterval;

            enemyfire::Shot shot;
            shot.speed = kOrbiterBulletSpeed;
            shot.damage = kOrbiterBulletDamage;

            // Tangent to the orbit at the current angle.
            const float tangent = m_angle + (m_angularSpeed >= 0.0f ? Pi * 0.5f : -Pi * 0.5f);
            enemyfire::fireAngle(world, next, tangent, shot);
        }
    }
}

void OrbiterBehavior::appendDraw(const EnemyBase& self, DrawList& out) const {
    // Faint tether back to the anchor so the cycloid reads as a deliberate path.
    const Vector2 mid = lerp(m_anchor, self.position(), 0.5f);
    Color tether{ 0.5f, 0.8f, 1.0f, kOrbiterTetherAlpha };
    out.add(SpriteId::White, mid, { m_orbitRadius, kOrbiterTetherSize }, DrawLayer::EnemyTrail,
            tether, m_angle, true);
}

// ===========================================================================
// Mine
//   Movement: barely moves -- a slow leftward drift with a shallow bob.
//   Weapon:   none until it dies. Proximity or damage arms a telegraphed
//             countdown, then it detonates into a ring of bullets.
// ===========================================================================

void MineBehavior::onSpawn(EnemyBase& self, const EnemySpawnParams& params) {
    m_bobPhase = params.phase;
    self.setVelocity({ kMineDrift * params.speedScale, 0.0f });
}

void MineBehavior::update(EnemyBase& self, float dt, IGameWorld& world) {
    m_bobPhase += dt * kMineBobRate;
    self.setVelocity({ kMineDrift * self.speedScale(),
                       std::cos(m_bobPhase) * kMineBobAmplitude * kMineBobRate });
    self.setRotation(std::sin(m_bobPhase * 0.4f) * 0.5f);

    if (m_armTimer > 0.0f) {
        m_armTimer -= dt;
        m_pulse += dt * kMineArmPulseRate;
        if (m_armTimer <= 0.0f) {
            // destroy() runs onDeath(), which lays the ring.
            self.destroy(world);
        }
        return;
    }

    const float proximity = distance(self.position(), world.playerPosition());
    if (proximity <= kMineProximityRadius) {
        m_armTimer = kMineArmDuration;
        m_pulse = 0.0f;
        HU_LOG_DEBUG(kLogCategory, "Mine armed by proximity at (%.0f, %.0f)",
                     self.position().x, self.position().y);
        EffectRequest fx;
        fx.kind = EffectKind::SuperweaponCharge;
        fx.position = self.position();
        fx.scale = 0.7f;
        fx.tint = Color{ 1.0f, 0.35f, 0.2f, 1.0f };
        world.spawnEffect(fx);
    }
}

void MineBehavior::onDeath(EnemyBase& self, IGameWorld& world) {
    if (m_detonated) {
        return;
    }
    m_detonated = true;

    enemyfire::Shot shot;
    shot.damage = kMineRingDamage;
    shot.radius = 6.0f;
    shot.size = { 14.0f, 14.0f };
    shot.tint = Color{ 1.0f, 0.6f, 0.35f, 1.0f };
    shot.additive = true;

    const float phase = randomRange(0.0f, TwoPi);
    int count = 0;

    if (enemyfire::isBulletHell(world)) {
        // Slow outer ring first, then a faster one rotated half a step so it
        // overtakes through the outer ring's gaps.
        shot.speed = kMineOuterRingSpeed;
        enemyfire::fireRing(world, self.position(), kMineOuterRingCount, phase, shot);

        const float halfStep = Pi / static_cast<float>(kMineInnerRingCount);
        shot.speed = kMineInnerRingSpeed;
        shot.tint = Color{ 1.0f, 0.85f, 0.45f, 1.0f };
        enemyfire::fireRing(world, self.position(), kMineInnerRingCount, phase + halfStep, shot);

        count = kMineOuterRingCount + kMineInnerRingCount;
    } else {
        count = static_cast<int>(kMineRingCount);
        shot.speed = kMineRingSpeed;
        enemyfire::fireRing(world, self.position(), count, phase, shot);
    }

    world.addScreenShake(0.4f, 0.22f);

    HU_LOG_DEBUG(kLogCategory, "Mine detonated: %d bullets at (%.0f, %.0f)", count,
                 self.position().x, self.position().y);
}

void MineBehavior::appendDraw(const EnemyBase& self, DrawList& out) const {
    if (m_armTimer <= 0.0f) {
        return;
    }
    // Telegraph: a glow that pulses faster and brighter as the fuse runs out.
    const float t = 1.0f - clampf(m_armTimer / kMineArmDuration, 0.0f, 1.0f);
    const float flash = 0.5f + 0.5f * std::sin(m_pulse);
    const float s = self.size().x * kMineGlowScale * (0.8f + 0.5f * t);
    Color glow{ 1.0f, 0.35f + 0.4f * flash, 0.2f, 0.25f + 0.55f * t * flash };
    out.add(SpriteId::ParticleRing, self.position(), { s, s }, DrawLayer::Particle, glow, 0.0f,
            true);
}

} // namespace hu
