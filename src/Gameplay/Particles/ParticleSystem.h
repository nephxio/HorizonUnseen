#pragma once

// Pooled, allocation-free particle simulation.
//
// The pool is sized once at construction and never grows. Emission is
// declarative: an effect describes RANGES (speed, angle, lifetime, size,
// colour) via ParticleSpec and asks for N particles, so the effect library
// stays data-shaped rather than being a pile of hand-rolled loops.
//
// When the pool is full a new particle recycles the OLDEST live particle
// instead of being dropped. Dropping makes a big explosion silently thin out;
// recycling keeps the newest, most visually important burst intact.

#include "Core/DrawList.h"
#include "Core/Math.h"
#include "Core/SpriteId.h"
#include "Core/Vector2.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace hu {

// ---------------------------------------------------------------------------
// Tunable constants
// ---------------------------------------------------------------------------
namespace particleconst {

inline constexpr std::size_t DefaultCapacity = 8192;
inline constexpr std::size_t MinCapacity = 64;

// Particles smaller than this are not worth a draw call.
inline constexpr float MinDrawSize = 0.05f;
// Below this alpha the quad contributes nothing visible.
inline constexpr float MinDrawAlpha = 0.004f;
// Drag is applied as v *= (1 - drag*dt), clamped so a large dt cannot flip the
// velocity sign.
inline constexpr float MaxDragFactorPerStep = 0.95f;
// Guard against pathological frame hitches integrating particles to infinity.
inline constexpr float MaxStepSeconds = 0.1f;

inline constexpr const char* LogCategory = "Particles";

} // namespace particleconst

// ---------------------------------------------------------------------------
// Emission description
// ---------------------------------------------------------------------------

// Every "Min/Max" pair is sampled uniformly per particle. Setting both to the
// same value makes the property deterministic.
struct ParticleSpec {
    // --- Origin ---
    Vector2 origin{ 0.0f, 0.0f };
    float spawnRadiusMin = 0.0f;      // Ring/disc spawn around the origin.
    float spawnRadiusMax = 0.0f;

    // --- Motion ---
    float angleMin = 0.0f;            // Radians; the emission cone.
    float angleMax = TwoPi;
    float speedMin = 0.0f;            // Along the sampled angle. Negative
    float speedMax = 0.0f;            // speeds travel inward (converging).
    float tangentialSpeedMin = 0.0f;  // Perpendicular component; non-zero
    float tangentialSpeedMax = 0.0f;  // values produce spirals.
    Vector2 acceleration{ 0.0f, 0.0f };  // Constant world accel (gravity/drift).
    float radialAccelMin = 0.0f;      // Along the spawn direction: positive
    float radialAccelMax = 0.0f;      // blows outward, negative pulls inward.
    float drag = 0.0f;                // Per-second velocity damping, 0 = none.

    // --- Life ---
    float lifetimeMin = 0.5f;
    float lifetimeMax = 1.0f;

    // --- Size ---
    float startSizeMin = 4.0f;
    float startSizeMax = 8.0f;
    float endSizeScaleMin = 1.0f;     // Multiplier of the start size at death.
    float endSizeScaleMax = 1.0f;
    float aspect = 1.0f;              // Width multiplier (>1 = elongated).

    // --- Colour (RGBA lerped from start to end over life) ---
    Color startColorA{ 1.0f, 1.0f, 1.0f, 1.0f };  // Start is a random lerp
    Color startColorB{ 1.0f, 1.0f, 1.0f, 1.0f };  // between A and B.
    Color endColorA{ 1.0f, 1.0f, 1.0f, 0.0f };
    Color endColorB{ 1.0f, 1.0f, 1.0f, 0.0f };

    // --- Rotation ---
    float rotationMin = 0.0f;
    float rotationMax = 0.0f;
    float angularVelocityMin = 0.0f;  // Radians/second.
    float angularVelocityMax = 0.0f;
    bool alignToVelocity = false;     // Overrides rotation each frame.

    // --- Appearance ---
    SpriteId sprite = SpriteId::ParticleSpark;
    bool additive = true;
};

// ---------------------------------------------------------------------------
// Simulation
// ---------------------------------------------------------------------------

class ParticleSystem {
public:
    explicit ParticleSystem(std::size_t capacity = particleconst::DefaultCapacity);

    // Spawns `count` particles from `spec`. Counts <= 0 are ignored. Never
    // allocates; recycles the oldest live particle once the pool is full.
    void emit(const ParticleSpec& spec, int count);

    // Integrates motion, ages particles and retires expired ones by swapping
    // the last live particle into the freed slot.
    void update(float dt);

    // Appends one SpriteInstance per live particle at DrawLayer::Particle.
    void appendDraw(DrawList& out) const;

    void clear();

    std::size_t liveCount() const { return m_liveCount; }
    std::size_t capacity() const { return m_particles.size(); }
    bool isFull() const { return m_liveCount >= m_particles.size(); }

private:
    struct Particle {
        Vector2 position{ 0.0f, 0.0f };
        Vector2 velocity{ 0.0f, 0.0f };
        Vector2 acceleration{ 0.0f, 0.0f };
        Vector2 startSize{ 1.0f, 1.0f };
        Vector2 endSize{ 1.0f, 1.0f };
        Color startColor{};
        Color endColor{};
        float rotation = 0.0f;
        float angularVelocity = 0.0f;
        float age = 0.0f;
        float lifetime = 1.0f;
        float invLifetime = 1.0f;
        float drag = 0.0f;
        SpriteId sprite = SpriteId::ParticleSpark;
        bool additive = true;
        bool alignToVelocity = false;
        // Monotonic spawn order, used to identify the oldest particle when the
        // pool overflows (indices are shuffled by swap compaction).
        std::uint64_t sequence = 0;
    };

    // Fills `out` from `spec`. Shared by the fresh-slot and recycle paths.
    void initParticle(Particle& out, const ParticleSpec& spec);

    // Linear scan for the smallest sequence number. Only walked on the
    // exceptional overflow path, never in the steady state.
    std::size_t oldestIndex() const;

    std::vector<Particle> m_particles;
    std::size_t m_liveCount = 0;
    std::uint64_t m_nextSequence = 1;
    bool m_loggedExhaustion = false;  // Pool-full warning is logged once.
};

} // namespace hu
