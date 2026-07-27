#include "Gameplay/Particles/EffectLibrary.h"

#include "Core/Log.h"
#include "Core/Math.h"
#include "Gameplay/Particles/ParticleSystem.h"

#include <algorithm>
#include <cmath>

namespace hu {
namespace {

// ---------------------------------------------------------------------------
// Tunable constants
// ---------------------------------------------------------------------------

constexpr const char* LogCategory = "Particles";

// request.scale is clamped so a bad caller cannot flood the pool or emit
// zero-size quads.
constexpr float MinEffectScale = 0.1f;
constexpr float MaxEffectScale = 8.0f;
constexpr int MaxEmissionCount = 512;

// Thruster is called every frame while moving, so it stays tiny by design.
constexpr int ThrusterBaseCount = 2;
constexpr int ThrusterMaxCount = 3;

// Shared palette pieces. Effects tint these with request.tint.
constexpr Color HotWhite{ 1.0f, 1.0f, 0.96f, 1.0f };
constexpr Color WarmYellow{ 1.0f, 0.86f, 0.42f, 1.0f };
constexpr Color EmberOrange{ 1.0f, 0.45f, 0.12f, 1.0f };
constexpr Color HarshRed{ 1.0f, 0.16f, 0.12f, 1.0f };
constexpr Color CoolCyan{ 0.45f, 0.92f, 1.0f, 1.0f };
constexpr Color DeepBlue{ 0.24f, 0.45f, 1.0f, 1.0f };
constexpr Color SmokeGrey{ 0.34f, 0.33f, 0.36f, 0.55f };
constexpr Color Transparent{ 1.0f, 1.0f, 1.0f, 0.0f };

// Smoke drifts up and back, as if the ship is still moving forward.
constexpr Vector2 SmokeDrift{ -18.0f, -26.0f };
constexpr Vector2 DebrisGravity{ 0.0f, 140.0f };

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

float effectScale(const EffectRequest& request) {
    return clampf(request.scale, MinEffectScale, MaxEffectScale);
}

int scaledCount(int base, float scale) {
    const int count = static_cast<int>(std::lround(static_cast<double>(base) * scale));
    return std::min(std::max(count, 1), MaxEmissionCount);
}

// Direction of travel for the effect; falls back to "forward" when the caller
// left it zeroed.
Vector2 safeDirection(const EffectRequest& request) {
    const Vector2 dir = normalize(request.direction);
    if (lengthSquared(dir) < 0.5f) {
        return Vector2{ 1.0f, 0.0f };
    }
    return dir;
}

Color withAlpha(Color c, float a) {
    c.a = a;
    return c;
}

// Multiplies two colours channel-wise; used to push a palette entry toward the
// caller's tint without losing the palette's character.
Color modulate(Color base, Color tint) {
    return Color{ base.r * tint.r, base.g * tint.g, base.b * tint.b, base.a * tint.a };
}

// Blends toward white; t=1 is pure white at the source alpha.
Color brighten(Color c, float t) {
    return Color{ lerp(c.r, 1.0f, t), lerp(c.g, 1.0f, t), lerp(c.b, 1.0f, t), c.a };
}

// ---------------------------------------------------------------------------
// Effect recipes
// ---------------------------------------------------------------------------

void playMuzzleFlash(ParticleSystem& particles, const EffectRequest& r) {
    const float s = effectScale(r);
    const float angle = angleOf(safeDirection(r));

    // Tight bright cone of very short-lived sparks.
    ParticleSpec sparks;
    sparks.origin = r.position;
    sparks.angleMin = angle - 0.26f;
    sparks.angleMax = angle + 0.26f;
    sparks.speedMin = 280.0f;
    sparks.speedMax = 620.0f;
    sparks.drag = 6.0f;
    sparks.lifetimeMin = 0.04f;
    sparks.lifetimeMax = 0.11f;
    sparks.startSizeMin = 5.0f * s;
    sparks.startSizeMax = 11.0f * s;
    sparks.endSizeScaleMin = 0.15f;
    sparks.endSizeScaleMax = 0.35f;
    sparks.aspect = 2.4f;
    sparks.alignToVelocity = true;
    sparks.startColorA = HotWhite;
    sparks.startColorB = brighten(modulate(WarmYellow, r.tint), 0.35f);
    sparks.endColorA = withAlpha(modulate(EmberOrange, r.tint), 0.0f);
    sparks.endColorB = Transparent;
    sparks.sprite = SpriteId::ParticleSpark;
    sparks.additive = true;
    particles.emit(sparks, scaledCount(7, s));

    // A single glow sitting on the barrel sells the light, not the debris.
    ParticleSpec glow;
    glow.origin = r.position;
    glow.speedMin = 0.0f;
    glow.speedMax = 0.0f;
    glow.lifetimeMin = 0.05f;
    glow.lifetimeMax = 0.09f;
    glow.startSizeMin = 16.0f * s;
    glow.startSizeMax = 22.0f * s;
    glow.endSizeScaleMin = 1.5f;
    glow.endSizeScaleMax = 1.9f;
    glow.startColorA = HotWhite;
    glow.startColorB = brighten(modulate(WarmYellow, r.tint), 0.5f);
    glow.endColorA = Transparent;
    glow.endColorB = Transparent;
    glow.sprite = SpriteId::ParticleGlow;
    glow.additive = true;
    particles.emit(glow, 1);
}

void playImpact(ParticleSystem& particles, const EffectRequest& r) {
    const float s = effectScale(r);
    // Sparks spray back toward whatever fired the shot.
    const float back = angleOf(safeDirection(r)) + Pi;

    ParticleSpec sparks;
    sparks.origin = r.position;
    sparks.angleMin = back - 0.95f;
    sparks.angleMax = back + 0.95f;
    sparks.speedMin = 120.0f;
    sparks.speedMax = 420.0f;
    sparks.drag = 3.5f;
    sparks.lifetimeMin = 0.12f;
    sparks.lifetimeMax = 0.34f;
    sparks.startSizeMin = 3.0f * s;
    sparks.startSizeMax = 7.0f * s;
    sparks.endSizeScaleMin = 0.2f;
    sparks.endSizeScaleMax = 0.5f;
    sparks.aspect = 2.0f;
    sparks.alignToVelocity = true;
    sparks.startColorA = HotWhite;
    sparks.startColorB = brighten(modulate(WarmYellow, r.tint), 0.2f);
    sparks.endColorA = withAlpha(modulate(EmberOrange, r.tint), 0.0f);
    sparks.endColorB = Transparent;
    sparks.sprite = SpriteId::ParticleSpark;
    sparks.additive = true;
    particles.emit(sparks, scaledCount(11, s));

    ParticleSpec shards;
    shards.origin = r.position;
    shards.angleMin = back - 1.4f;
    shards.angleMax = back + 1.4f;
    shards.speedMin = 90.0f;
    shards.speedMax = 260.0f;
    shards.drag = 2.2f;
    shards.lifetimeMin = 0.25f;
    shards.lifetimeMax = 0.55f;
    shards.startSizeMin = 3.0f * s;
    shards.startSizeMax = 6.0f * s;
    shards.endSizeScaleMin = 0.7f;
    shards.endSizeScaleMax = 1.0f;
    shards.aspect = 1.6f;
    shards.rotationMin = 0.0f;
    shards.rotationMax = TwoPi;
    shards.angularVelocityMin = -9.0f;
    shards.angularVelocityMax = 9.0f;
    shards.startColorA = modulate(WarmYellow, r.tint);
    shards.startColorB = modulate(EmberOrange, r.tint);
    shards.endColorA = Transparent;
    shards.endColorB = Transparent;
    shards.sprite = SpriteId::ParticleShard;
    shards.additive = false;
    particles.emit(shards, scaledCount(4, s));
}

void playExplosion(ParticleSystem& particles, const EffectRequest& r) {
    const float s = effectScale(r);

    // Core: one expanding additive glow that does most of the visual work.
    ParticleSpec core;
    core.origin = r.position;
    core.spawnRadiusMax = 3.0f * s;
    core.lifetimeMin = 0.28f;
    core.lifetimeMax = 0.42f;
    core.startSizeMin = 34.0f * s;
    core.startSizeMax = 46.0f * s;
    core.endSizeScaleMin = 2.0f;
    core.endSizeScaleMax = 2.6f;
    core.startColorA = HotWhite;
    core.startColorB = brighten(modulate(WarmYellow, r.tint), 0.4f);
    core.endColorA = withAlpha(modulate(EmberOrange, r.tint), 0.0f);
    core.endColorB = Transparent;
    core.sprite = SpriteId::ParticleGlow;
    core.additive = true;
    particles.emit(core, 2);

    ParticleSpec sparks;
    sparks.origin = r.position;
    sparks.spawnRadiusMax = 6.0f * s;
    sparks.speedMin = 90.0f;
    sparks.speedMax = 440.0f;
    sparks.drag = 1.6f;
    sparks.lifetimeMin = 0.3f;
    sparks.lifetimeMax = 0.75f;
    sparks.startSizeMin = 4.0f * s;
    sparks.startSizeMax = 9.0f * s;
    sparks.endSizeScaleMin = 0.15f;
    sparks.endSizeScaleMax = 0.4f;
    sparks.aspect = 2.2f;
    sparks.alignToVelocity = true;
    sparks.startColorA = HotWhite;
    sparks.startColorB = modulate(WarmYellow, r.tint);
    sparks.endColorA = withAlpha(modulate(HarshRed, r.tint), 0.0f);
    sparks.endColorB = Transparent;
    sparks.sprite = SpriteId::ParticleSpark;
    sparks.additive = true;
    particles.emit(sparks, scaledCount(26, s));

    ParticleSpec smoke;
    smoke.origin = r.position;
    smoke.spawnRadiusMax = 10.0f * s;
    smoke.speedMin = 15.0f;
    smoke.speedMax = 80.0f;
    smoke.acceleration = SmokeDrift;
    smoke.drag = 1.1f;
    smoke.lifetimeMin = 0.8f;
    smoke.lifetimeMax = 1.6f;
    smoke.startSizeMin = 16.0f * s;
    smoke.startSizeMax = 30.0f * s;
    smoke.endSizeScaleMin = 1.9f;
    smoke.endSizeScaleMax = 2.8f;
    smoke.rotationMin = 0.0f;
    smoke.rotationMax = TwoPi;
    smoke.angularVelocityMin = -1.2f;
    smoke.angularVelocityMax = 1.2f;
    smoke.startColorA = SmokeGrey;
    smoke.startColorB = withAlpha(modulate(EmberOrange, r.tint), 0.45f);
    smoke.endColorA = withAlpha(SmokeGrey, 0.0f);
    smoke.endColorB = withAlpha(SmokeGrey, 0.0f);
    smoke.sprite = SpriteId::ParticleSmoke;
    smoke.additive = false;  // Smoke must occlude, not glow.
    particles.emit(smoke, scaledCount(9, s));
}

void playBigExplosion(ParticleSystem& particles, const EffectRequest& r) {
    const float s = effectScale(r);

    ParticleSpec flash;
    flash.origin = r.position;
    flash.spawnRadiusMax = 8.0f * s;
    flash.lifetimeMin = 0.4f;
    flash.lifetimeMax = 0.6f;
    flash.startSizeMin = 80.0f * s;
    flash.startSizeMax = 110.0f * s;
    flash.endSizeScaleMin = 2.6f;
    flash.endSizeScaleMax = 3.4f;
    flash.startColorA = HotWhite;
    flash.startColorB = HotWhite;
    flash.endColorA = withAlpha(modulate(EmberOrange, r.tint), 0.0f);
    flash.endColorB = Transparent;
    flash.sprite = SpriteId::ParticleGlow;
    flash.additive = true;
    particles.emit(flash, 3);

    // Shockwave: a ring quad scaled up hard over its life.
    ParticleSpec ring;
    ring.origin = r.position;
    ring.lifetimeMin = 0.55f;
    ring.lifetimeMax = 0.8f;
    ring.startSizeMin = 40.0f * s;
    ring.startSizeMax = 50.0f * s;
    ring.endSizeScaleMin = 8.0f;
    ring.endSizeScaleMax = 10.0f;
    ring.startColorA = HotWhite;
    ring.startColorB = brighten(modulate(WarmYellow, r.tint), 0.3f);
    ring.endColorA = Transparent;
    ring.endColorB = Transparent;
    ring.sprite = SpriteId::ParticleRing;
    ring.additive = true;
    particles.emit(ring, 2);

    ParticleSpec sparks;
    sparks.origin = r.position;
    sparks.spawnRadiusMax = 14.0f * s;
    sparks.speedMin = 160.0f;
    sparks.speedMax = 820.0f;
    sparks.drag = 1.2f;
    sparks.lifetimeMin = 0.4f;
    sparks.lifetimeMax = 1.1f;
    sparks.startSizeMin = 5.0f * s;
    sparks.startSizeMax = 12.0f * s;
    sparks.endSizeScaleMin = 0.1f;
    sparks.endSizeScaleMax = 0.35f;
    sparks.aspect = 2.8f;
    sparks.alignToVelocity = true;
    sparks.startColorA = HotWhite;
    sparks.startColorB = modulate(WarmYellow, r.tint);
    sparks.endColorA = withAlpha(modulate(HarshRed, r.tint), 0.0f);
    sparks.endColorB = Transparent;
    sparks.sprite = SpriteId::ParticleSpark;
    sparks.additive = true;
    particles.emit(sparks, scaledCount(60, s));

    ParticleSpec shards;
    shards.origin = r.position;
    shards.spawnRadiusMax = 12.0f * s;
    shards.speedMin = 120.0f;
    shards.speedMax = 520.0f;
    shards.drag = 1.0f;
    shards.acceleration = DebrisGravity;
    shards.lifetimeMin = 0.6f;
    shards.lifetimeMax = 1.4f;
    shards.startSizeMin = 6.0f * s;
    shards.startSizeMax = 14.0f * s;
    shards.endSizeScaleMin = 0.8f;
    shards.endSizeScaleMax = 1.0f;
    shards.aspect = 1.8f;
    shards.rotationMin = 0.0f;
    shards.rotationMax = TwoPi;
    shards.angularVelocityMin = -12.0f;
    shards.angularVelocityMax = 12.0f;
    shards.startColorA = modulate(WarmYellow, r.tint);
    shards.startColorB = modulate(EmberOrange, r.tint);
    shards.endColorA = withAlpha(SmokeGrey, 0.0f);
    shards.endColorB = Transparent;
    shards.sprite = SpriteId::ParticleShard;
    shards.additive = false;
    particles.emit(shards, scaledCount(30, s));

    ParticleSpec smoke;
    smoke.origin = r.position;
    smoke.spawnRadiusMax = 26.0f * s;
    smoke.speedMin = 10.0f;
    smoke.speedMax = 110.0f;
    smoke.acceleration = SmokeDrift;
    smoke.drag = 0.9f;
    smoke.lifetimeMin = 1.4f;
    smoke.lifetimeMax = 2.8f;
    smoke.startSizeMin = 30.0f * s;
    smoke.startSizeMax = 58.0f * s;
    smoke.endSizeScaleMin = 2.2f;
    smoke.endSizeScaleMax = 3.2f;
    smoke.rotationMin = 0.0f;
    smoke.rotationMax = TwoPi;
    smoke.angularVelocityMin = -0.9f;
    smoke.angularVelocityMax = 0.9f;
    smoke.startColorA = SmokeGrey;
    smoke.startColorB = withAlpha(modulate(EmberOrange, r.tint), 0.4f);
    smoke.endColorA = withAlpha(SmokeGrey, 0.0f);
    smoke.endColorB = withAlpha(SmokeGrey, 0.0f);
    smoke.sprite = SpriteId::ParticleSmoke;
    smoke.additive = false;
    particles.emit(smoke, scaledCount(22, s));
}

void playPowerupPickup(ParticleSystem& particles, const EffectRequest& r) {
    const float s = effectScale(r);

    ParticleSpec ring;
    ring.origin = r.position;
    ring.lifetimeMin = 0.35f;
    ring.lifetimeMax = 0.5f;
    ring.startSizeMin = 14.0f * s;
    ring.startSizeMax = 18.0f * s;
    ring.endSizeScaleMin = 4.0f;
    ring.endSizeScaleMax = 5.0f;
    ring.startColorA = brighten(r.tint, 0.5f);
    ring.startColorB = HotWhite;
    ring.endColorA = withAlpha(r.tint, 0.0f);
    ring.endColorB = Transparent;
    ring.sprite = SpriteId::ParticleRing;
    ring.additive = true;
    particles.emit(ring, 2);

    // Cheerful rising motes: upward acceleration is what reads as "collected".
    ParticleSpec sparks;
    sparks.origin = r.position;
    sparks.spawnRadiusMax = 12.0f * s;
    sparks.speedMin = 40.0f;
    sparks.speedMax = 170.0f;
    sparks.acceleration = Vector2{ 0.0f, -240.0f };
    sparks.drag = 1.4f;
    sparks.lifetimeMin = 0.4f;
    sparks.lifetimeMax = 0.9f;
    sparks.startSizeMin = 4.0f * s;
    sparks.startSizeMax = 9.0f * s;
    sparks.endSizeScaleMin = 0.2f;
    sparks.endSizeScaleMax = 0.5f;
    sparks.startColorA = brighten(r.tint, 0.35f);
    sparks.startColorB = HotWhite;
    sparks.endColorA = withAlpha(r.tint, 0.0f);
    sparks.endColorB = Transparent;
    sparks.sprite = SpriteId::ParticleSpark;
    sparks.additive = true;
    particles.emit(sparks, scaledCount(18, s));
}

void playCellBreak(ParticleSystem& particles, const EffectRequest& r) {
    const float s = effectScale(r);

    // Sharp, short flash: it snaps rather than blooms.
    ParticleSpec flash;
    flash.origin = r.position;
    flash.lifetimeMin = 0.08f;
    flash.lifetimeMax = 0.14f;
    flash.startSizeMin = 30.0f * s;
    flash.startSizeMax = 40.0f * s;
    flash.endSizeScaleMin = 0.35f;
    flash.endSizeScaleMax = 0.55f;
    flash.startColorA = HotWhite;
    flash.startColorB = HotWhite;
    flash.endColorA = withAlpha(HarshRed, 0.0f);
    flash.endColorB = Transparent;
    flash.sprite = SpriteId::ParticleGlow;
    flash.additive = true;
    particles.emit(flash, 2);

    // Fast outward shards: hard, angular, red/white. Reads as "shattered".
    ParticleSpec shards;
    shards.origin = r.position;
    shards.spawnRadiusMax = 6.0f * s;
    shards.speedMin = 340.0f;
    shards.speedMax = 780.0f;
    shards.drag = 1.1f;
    shards.lifetimeMin = 0.22f;
    shards.lifetimeMax = 0.55f;
    shards.startSizeMin = 5.0f * s;
    shards.startSizeMax = 12.0f * s;
    shards.endSizeScaleMin = 0.5f;
    shards.endSizeScaleMax = 0.8f;
    shards.aspect = 2.6f;
    shards.rotationMin = 0.0f;
    shards.rotationMax = TwoPi;
    shards.angularVelocityMin = -16.0f;
    shards.angularVelocityMax = 16.0f;
    shards.startColorA = HotWhite;
    shards.startColorB = HarshRed;
    shards.endColorA = withAlpha(HarshRed, 0.0f);
    shards.endColorB = Transparent;
    shards.sprite = SpriteId::ParticleShard;
    shards.additive = false;
    particles.emit(shards, scaledCount(22, s));

    ParticleSpec sparks;
    sparks.origin = r.position;
    sparks.speedMin = 260.0f;
    sparks.speedMax = 900.0f;
    sparks.drag = 2.4f;
    sparks.lifetimeMin = 0.12f;
    sparks.lifetimeMax = 0.3f;
    sparks.startSizeMin = 4.0f * s;
    sparks.startSizeMax = 8.0f * s;
    sparks.endSizeScaleMin = 0.1f;
    sparks.endSizeScaleMax = 0.3f;
    sparks.aspect = 3.0f;
    sparks.alignToVelocity = true;
    sparks.startColorA = HotWhite;
    sparks.startColorB = HarshRed;
    sparks.endColorA = Transparent;
    sparks.endColorB = Transparent;
    sparks.sprite = SpriteId::ParticleSpark;
    sparks.additive = true;
    particles.emit(sparks, scaledCount(16, s));
}

void playCellCharge(ParticleSystem& particles, const EffectRequest& r) {
    const float s = effectScale(r);

    // Motes spawn on a ring and drift inward and upward: soft, unhurried.
    ParticleSpec motes;
    motes.origin = r.position;
    motes.spawnRadiusMin = 40.0f * s;
    motes.spawnRadiusMax = 90.0f * s;
    motes.speedMin = -140.0f;  // Negative speed converges on the origin.
    motes.speedMax = -60.0f;
    motes.acceleration = Vector2{ 0.0f, -50.0f };
    motes.drag = 0.4f;
    motes.lifetimeMin = 0.5f;
    motes.lifetimeMax = 0.95f;
    motes.startSizeMin = 4.0f * s;
    motes.startSizeMax = 10.0f * s;
    motes.endSizeScaleMin = 0.2f;
    motes.endSizeScaleMax = 0.45f;
    motes.startColorA = withAlpha(CoolCyan, 0.85f);
    motes.startColorB = withAlpha(DeepBlue, 0.85f);
    motes.endColorA = withAlpha(CoolCyan, 0.0f);
    motes.endColorB = Transparent;
    motes.sprite = SpriteId::ParticleGlow;
    motes.additive = true;
    particles.emit(motes, scaledCount(14, s));
}

void playSuperweaponCharge(ParticleSystem& particles, const EffectRequest& r) {
    const float s = effectScale(r);

    // Tangential velocity plus inward radial acceleration gives the spiral.
    // Lifetimes are tuned so a stream of these reads as a ~0.5s wind-up.
    ParticleSpec motes;
    motes.origin = r.position;
    motes.spawnRadiusMin = 90.0f * s;
    motes.spawnRadiusMax = 170.0f * s;
    motes.speedMin = -300.0f;
    motes.speedMax = -180.0f;
    motes.tangentialSpeedMin = 130.0f;
    motes.tangentialSpeedMax = 280.0f;
    motes.radialAccelMin = -320.0f;
    motes.radialAccelMax = -180.0f;
    motes.lifetimeMin = 0.4f;
    motes.lifetimeMax = 0.6f;
    motes.startSizeMin = 5.0f * s;
    motes.startSizeMax = 11.0f * s;
    motes.endSizeScaleMin = 0.15f;
    motes.endSizeScaleMax = 0.4f;
    motes.aspect = 2.2f;
    motes.alignToVelocity = true;
    motes.startColorA = brighten(modulate(CoolCyan, r.tint), 0.4f);
    motes.startColorB = HotWhite;
    motes.endColorA = withAlpha(modulate(DeepBlue, r.tint), 0.0f);
    motes.endColorB = Transparent;
    motes.sprite = SpriteId::ParticleSpark;
    motes.additive = true;
    particles.emit(motes, scaledCount(24, s));

    // Core that swells as the energy arrives.
    ParticleSpec core;
    core.origin = r.position;
    core.lifetimeMin = 0.45f;
    core.lifetimeMax = 0.55f;
    core.startSizeMin = 12.0f * s;
    core.startSizeMax = 18.0f * s;
    core.endSizeScaleMin = 1.8f;
    core.endSizeScaleMax = 2.3f;
    core.startColorA = withAlpha(modulate(CoolCyan, r.tint), 0.5f);
    core.startColorB = withAlpha(HotWhite, 0.7f);
    core.endColorA = withAlpha(HotWhite, 0.0f);
    core.endColorB = Transparent;
    core.sprite = SpriteId::ParticleGlow;
    core.additive = true;
    particles.emit(core, 1);
}

void playScreenClear(ParticleSystem& particles, const EffectRequest& r) {
    const float s = effectScale(r);

    // Energy Bomb. Deliberately the loudest thing in the game.
    ParticleSpec ring;
    ring.origin = r.position;
    ring.lifetimeMin = 0.8f;
    ring.lifetimeMax = 1.0f;
    ring.startSizeMin = 60.0f * s;
    ring.startSizeMax = 70.0f * s;
    ring.endSizeScaleMin = 22.0f;
    ring.endSizeScaleMax = 26.0f;
    ring.startColorA = HotWhite;
    ring.startColorB = brighten(CoolCyan, 0.5f);
    ring.endColorA = withAlpha(CoolCyan, 0.0f);
    ring.endColorB = Transparent;
    ring.sprite = SpriteId::ParticleRing;
    ring.additive = true;
    particles.emit(ring, 3);

    ParticleSpec flash;
    flash.origin = r.position;
    flash.lifetimeMin = 0.4f;
    flash.lifetimeMax = 0.6f;
    flash.startSizeMin = 180.0f * s;
    flash.startSizeMax = 240.0f * s;
    flash.endSizeScaleMin = 2.2f;
    flash.endSizeScaleMax = 2.8f;
    flash.startColorA = HotWhite;
    flash.startColorB = HotWhite;
    flash.endColorA = withAlpha(CoolCyan, 0.0f);
    flash.endColorB = Transparent;
    flash.sprite = SpriteId::ParticleGlow;
    flash.additive = true;
    particles.emit(flash, 2);

    ParticleSpec wave;
    wave.origin = r.position;
    wave.spawnRadiusMax = 24.0f * s;
    wave.speedMin = 700.0f;
    wave.speedMax = 1500.0f;
    wave.drag = 0.8f;
    wave.lifetimeMin = 0.55f;
    wave.lifetimeMax = 1.0f;
    wave.startSizeMin = 6.0f * s;
    wave.startSizeMax = 14.0f * s;
    wave.endSizeScaleMin = 0.15f;
    wave.endSizeScaleMax = 0.4f;
    wave.aspect = 3.2f;
    wave.alignToVelocity = true;
    wave.startColorA = HotWhite;
    wave.startColorB = brighten(CoolCyan, 0.3f);
    wave.endColorA = withAlpha(DeepBlue, 0.0f);
    wave.endColorB = Transparent;
    wave.sprite = SpriteId::ParticleSpark;
    wave.additive = true;
    particles.emit(wave, scaledCount(130, s));
}

void playThruster(ParticleSystem& particles, const EffectRequest& r) {
    const float s = effectScale(r);
    // Trails opposite the direction of travel.
    const float back = angleOf(safeDirection(r)) + Pi;

    ParticleSpec trail;
    trail.origin = r.position;
    trail.spawnRadiusMax = 3.0f * s;
    trail.angleMin = back - 0.32f;
    trail.angleMax = back + 0.32f;
    trail.speedMin = 60.0f;
    trail.speedMax = 190.0f;
    trail.drag = 3.0f;
    trail.lifetimeMin = 0.1f;
    trail.lifetimeMax = 0.26f;
    trail.startSizeMin = 4.0f * s;
    trail.startSizeMax = 9.0f * s;
    trail.endSizeScaleMin = 0.1f;
    trail.endSizeScaleMax = 0.3f;
    trail.aspect = 1.6f;
    trail.alignToVelocity = true;
    trail.startColorA = brighten(modulate(WarmYellow, r.tint), 0.4f);
    trail.startColorB = modulate(EmberOrange, r.tint);
    trail.endColorA = withAlpha(modulate(EmberOrange, r.tint), 0.0f);
    trail.endColorB = Transparent;
    trail.sprite = SpriteId::ParticleSpark;
    trail.additive = true;

    // Called every frame while the ship moves, so the per-call budget is tiny.
    const int count = std::min(scaledCount(ThrusterBaseCount, s), ThrusterMaxCount);
    particles.emit(trail, count);
}

void playDebris(ParticleSystem& particles, const EffectRequest& r) {
    const float s = effectScale(r);

    ParticleSpec shards;
    shards.origin = r.position;
    shards.spawnRadiusMax = 10.0f * s;
    shards.speedMin = 60.0f;
    shards.speedMax = 320.0f;
    shards.acceleration = DebrisGravity;
    shards.drag = 0.9f;
    shards.lifetimeMin = 1.0f;
    shards.lifetimeMax = 2.2f;
    shards.startSizeMin = 5.0f * s;
    shards.startSizeMax = 13.0f * s;
    shards.endSizeScaleMin = 0.75f;
    shards.endSizeScaleMax = 1.0f;
    shards.aspect = 1.7f;
    shards.rotationMin = 0.0f;
    shards.rotationMax = TwoPi;
    shards.angularVelocityMin = -8.0f;
    shards.angularVelocityMax = 8.0f;
    shards.startColorA = r.tint;
    shards.startColorB = modulate(SmokeGrey, brighten(r.tint, 0.4f));
    shards.endColorA = withAlpha(r.tint, 0.0f);
    shards.endColorB = Transparent;
    shards.sprite = SpriteId::ParticleShard;
    shards.additive = false;
    particles.emit(shards, scaledCount(12, s));
}

} // namespace

void playEffect(ParticleSystem& particles, const EffectRequest& request) {
    switch (request.kind) {
        case EffectKind::MuzzleFlash:       playMuzzleFlash(particles, request);       return;
        case EffectKind::Impact:            playImpact(particles, request);            return;
        case EffectKind::Explosion:         playExplosion(particles, request);         return;
        case EffectKind::BigExplosion:      playBigExplosion(particles, request);      return;
        case EffectKind::PowerupPickup:     playPowerupPickup(particles, request);     return;
        case EffectKind::CellBreak:         playCellBreak(particles, request);         return;
        case EffectKind::CellCharge:        playCellCharge(particles, request);        return;
        case EffectKind::SuperweaponCharge: playSuperweaponCharge(particles, request); return;
        case EffectKind::ScreenClear:       playScreenClear(particles, request);       return;
        case EffectKind::Thruster:          playThruster(particles, request);          return;
        case EffectKind::Debris:            playDebris(particles, request);            return;
    }

    // A newly added EffectKind that nobody wired up should be loud, not silent.
    HU_LOG_WARN(LogCategory, "playEffect: unhandled EffectKind %d, falling back to Impact",
                static_cast<int>(request.kind));
    playImpact(particles, request);
}

} // namespace hu
