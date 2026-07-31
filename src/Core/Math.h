#pragma once

// Small 2D vector helpers shared by gameplay and rendering.
//
// The type itself lives in Core/Vector2.h; this header adds free functions
// around it rather than introducing a competing vector class.

#include "Core/Vector2.h"

#include <cmath>
#include <cstdlib>

namespace hu {

constexpr float Pi = 3.14159265358979323846f;
constexpr float TwoPi = Pi * 2.0f;

inline Vector2 add(Vector2 a, Vector2 b) { return { a.x + b.x, a.y + b.y }; }
inline Vector2 sub(Vector2 a, Vector2 b) { return { a.x - b.x, a.y - b.y }; }
inline Vector2 scale(Vector2 v, float s) { return { v.x * s, v.y * s }; }

inline float dot(Vector2 a, Vector2 b) { return a.x * b.x + a.y * b.y; }
inline float lengthSquared(Vector2 v) { return v.x * v.x + v.y * v.y; }
inline float length(Vector2 v) { return std::sqrt(lengthSquared(v)); }

inline Vector2 normalize(Vector2 v) {
    const float len = length(v);
    // Zero-length vectors have no direction; returning zero keeps callers from
    // propagating NaNs into positions, which is very hard to debug later.
    if (len <= 1e-6f) {
        return { 0.0f, 0.0f };
    }
    return { v.x / len, v.y / len };
}

inline float distance(Vector2 a, Vector2 b) { return length(sub(b, a)); }
inline float distanceSquared(Vector2 a, Vector2 b) { return lengthSquared(sub(b, a)); }

inline float angleOf(Vector2 v) { return std::atan2(v.y, v.x); }
inline Vector2 fromAngle(float radians, float magnitude = 1.0f) {
    return { std::cos(radians) * magnitude, std::sin(radians) * magnitude };
}

inline Vector2 rotate(Vector2 v, float radians) {
    const float c = std::cos(radians);
    const float s = std::sin(radians);
    return { v.x * c - v.y * s, v.x * s + v.y * c };
}

inline float degreesToRadians(float degrees) { return degrees * (Pi / 180.0f); }
inline float radiansToDegrees(float radians) { return radians * (180.0f / Pi); }

inline float clampf(float value, float lo, float hi) {
    return value < lo ? lo : (value > hi ? hi : value);
}

inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline Vector2 lerp(Vector2 a, Vector2 b, float t) {
    return { lerp(a.x, b.x, t), lerp(a.y, b.y, t) };
}

// Shortest signed difference between two angles, wrapped to [-Pi, Pi]. Used by
// homing weapons so a missile turns the short way around toward its target.
inline float angleDifference(float from, float to) {
    float diff = to - from;
    while (diff > Pi) { diff -= TwoPi; }
    while (diff < -Pi) { diff += TwoPi; }
    return diff;
}

// Rotates `from` toward `to` by at most maxDelta radians.
inline float rotateTowards(float from, float to, float maxDelta) {
    const float diff = angleDifference(from, to);
    if (std::fabs(diff) <= maxDelta) {
        return to;
    }
    return from + (diff > 0.0f ? maxDelta : -maxDelta);
}

// Deterministic-enough randomness for effects. Gameplay systems that need
// reproducibility should own their own seeded generator instead.
inline float randomUnit() {
    return static_cast<float>(std::rand()) / static_cast<float>(RAND_MAX);
}

inline float randomRange(float lo, float hi) {
    return lo + randomUnit() * (hi - lo);
}

} // namespace hu
