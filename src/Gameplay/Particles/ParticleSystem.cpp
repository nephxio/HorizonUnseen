#include "Gameplay/Particles/ParticleSystem.h"

#include "Core/Log.h"

#include <algorithm>
#include <cmath>

namespace hu {
namespace {

using namespace particleconst;

Color lerpColor(const Color& a, const Color& b, float t) {
    return Color{ lerp(a.r, b.r, t), lerp(a.g, b.g, t),
                  lerp(a.b, b.b, t), lerp(a.a, b.a, t) };
}

Color randomColorBetween(const Color& a, const Color& b) {
    return lerpColor(a, b, randomUnit());
}

} // namespace

ParticleSystem::ParticleSystem(std::size_t capacity) {
    const std::size_t clamped = std::max(capacity, MinCapacity);
    // Sized once, here. Nothing below ever grows the vector, which is what
    // makes update/emit/appendDraw allocation-free.
    m_particles.resize(clamped);
    HU_LOG_INFO(LogCategory, "ParticleSystem initialised: capacity=%zu (%zu bytes)",
                clamped, clamped * sizeof(Particle));
}

void ParticleSystem::initParticle(Particle& out, const ParticleSpec& spec) {
    const float angle = randomRange(spec.angleMin, spec.angleMax);
    const Vector2 dir = fromAngle(angle);
    const Vector2 tangent{ -dir.y, dir.x };

    const float radius = randomRange(spec.spawnRadiusMin, spec.spawnRadiusMax);
    out.position = add(spec.origin, scale(dir, radius));

    const float speed = randomRange(spec.speedMin, spec.speedMax);
    const float tangential = randomRange(spec.tangentialSpeedMin, spec.tangentialSpeedMax);
    out.velocity = add(scale(dir, speed), scale(tangent, tangential));

    const float radialAccel = randomRange(spec.radialAccelMin, spec.radialAccelMax);
    out.acceleration = add(spec.acceleration, scale(dir, radialAccel));
    out.drag = spec.drag;

    const float lifetime = std::max(randomRange(spec.lifetimeMin, spec.lifetimeMax), 1e-4f);
    out.lifetime = lifetime;
    out.invLifetime = 1.0f / lifetime;
    out.age = 0.0f;

    const float startSize = std::max(randomRange(spec.startSizeMin, spec.startSizeMax), 0.0f);
    const float endScale = randomRange(spec.endSizeScaleMin, spec.endSizeScaleMax);
    const float aspect = spec.aspect <= 0.0f ? 1.0f : spec.aspect;
    out.startSize = Vector2{ startSize * aspect, startSize };
    out.endSize = Vector2{ startSize * aspect * endScale, startSize * endScale };

    out.startColor = randomColorBetween(spec.startColorA, spec.startColorB);
    out.endColor = randomColorBetween(spec.endColorA, spec.endColorB);

    out.rotation = randomRange(spec.rotationMin, spec.rotationMax);
    out.angularVelocity = randomRange(spec.angularVelocityMin, spec.angularVelocityMax);
    out.alignToVelocity = spec.alignToVelocity;
    if (out.alignToVelocity) {
        out.rotation = angleOf(out.velocity);
    }

    out.sprite = spec.sprite;
    out.additive = spec.additive;
    out.sequence = m_nextSequence++;
}

std::size_t ParticleSystem::oldestIndex() const {
    std::size_t best = 0;
    std::uint64_t bestSequence = m_particles[0].sequence;
    for (std::size_t i = 1; i < m_liveCount; ++i) {
        if (m_particles[i].sequence < bestSequence) {
            bestSequence = m_particles[i].sequence;
            best = i;
        }
    }
    return best;
}

void ParticleSystem::emit(const ParticleSpec& spec, int count) {
    if (count <= 0 || m_particles.empty()) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        std::size_t slot = 0;
        if (m_liveCount < m_particles.size()) {
            slot = m_liveCount++;
        } else {
            // Pool exhausted. Steal the oldest particle: the newest burst is
            // almost always the one the player is looking at.
            if (!m_loggedExhaustion) {
                m_loggedExhaustion = true;
                HU_LOG_WARN(LogCategory,
                            "Particle pool exhausted (capacity=%zu); recycling oldest particles. "
                            "This warning is logged once per session.",
                            m_particles.size());
            }
            slot = oldestIndex();
        }
        initParticle(m_particles[slot], spec);
    }
}

void ParticleSystem::update(float dt) {
    if (dt <= 0.0f) {
        return;
    }
    const float step = std::min(dt, MaxStepSeconds);

    std::size_t i = 0;
    while (i < m_liveCount) {
        Particle& p = m_particles[i];
        p.age += step;
        if (p.age >= p.lifetime) {
            // Swap-with-last compaction keeps the live set contiguous, so the
            // draw/update loops stay cache friendly and never test a dead flag.
            --m_liveCount;
            if (i != m_liveCount) {
                p = m_particles[m_liveCount];
            }
            continue;  // Re-test the particle just swapped into this slot.
        }

        p.velocity = add(p.velocity, scale(p.acceleration, step));
        if (p.drag > 0.0f) {
            const float damping = 1.0f - std::min(p.drag * step, MaxDragFactorPerStep);
            p.velocity = scale(p.velocity, damping);
        }
        p.position = add(p.position, scale(p.velocity, step));

        if (p.alignToVelocity) {
            if (lengthSquared(p.velocity) > 1e-6f) {
                p.rotation = angleOf(p.velocity);
            }
        } else {
            p.rotation += p.angularVelocity * step;
        }

        ++i;
    }
}

void ParticleSystem::appendDraw(DrawList& out) const {
    for (std::size_t i = 0; i < m_liveCount; ++i) {
        const Particle& p = m_particles[i];
        const float t = clampf(p.age * p.invLifetime, 0.0f, 1.0f);

        const Color color = lerpColor(p.startColor, p.endColor, t);
        if (color.a <= MinDrawAlpha) {
            continue;
        }

        const Vector2 size = lerp(p.startSize, p.endSize, t);
        if (size.x <= MinDrawSize || size.y <= MinDrawSize) {
            continue;
        }

        out.add(p.sprite, p.position, size, DrawLayer::Particle, color, p.rotation, p.additive);
    }
}

void ParticleSystem::clear() {
    m_liveCount = 0;
}

} // namespace hu
