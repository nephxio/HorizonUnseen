#pragma once

// Renderer-agnostic draw submission.
//
// Gameplay builds a DrawList each frame; the Vulkan layer consumes it. Keeping
// this as plain data means gameplay code never includes a graphics header, and
// the same scene can be drawn by the game window or the editor viewport.

#include "Core/SpriteId.h"
#include "Core/Vector2.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace hu {

struct Color {
    float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
};

inline constexpr Color White{ 1.0f, 1.0f, 1.0f, 1.0f };

// Draw order. Sprites are sorted by layer before submission, so gameplay can
// emit in any order it finds convenient.
enum class DrawLayer : std::int16_t {
    BackgroundFar = 0,
    BackgroundNear = 10,
    EnemyTrail = 20,
    Powerup = 30,
    Enemy = 40,
    Player = 50,
    PlayerProjectile = 60,
    EnemyProjectile = 70,
    Beam = 80,
    Particle = 90,
    Foreground = 100
};

struct SpriteInstance {
    Vector2 position{ 0.0f, 0.0f };   // Centre of the quad, in world pixels.
    Vector2 size{ 32.0f, 32.0f };     // Full width/height in world pixels.
    float rotation = 0.0f;            // Radians, counter-clockwise.
    Color color{};                    // Multiplied with the texture sample.
    SpriteId sprite = SpriteId::White;
    DrawLayer layer = DrawLayer::Player;
    // Additive blending suits energy effects (lasers, sparks, explosions);
    // alpha blending suits ships and power-ups.
    bool additive = false;
};

class DrawList {
public:
    void clear() { m_sprites.clear(); }

    void add(const SpriteInstance& instance) { m_sprites.push_back(instance); }

    void add(SpriteId sprite, Vector2 position, Vector2 size, DrawLayer layer,
             Color color = White, float rotation = 0.0f, bool additive = false) {
        SpriteInstance instance;
        instance.sprite = sprite;
        instance.position = position;
        instance.size = size;
        instance.layer = layer;
        instance.color = color;
        instance.rotation = rotation;
        instance.additive = additive;
        m_sprites.push_back(instance);
    }

    // Stable sort keeps emission order within a layer, which matters for
    // effects that are drawn as a sequence of overlapping quads.
    void sortByLayer() {
        std::stable_sort(m_sprites.begin(), m_sprites.end(),
                         [](const SpriteInstance& a, const SpriteInstance& b) {
                             return a.layer < b.layer;
                         });
    }

    const std::vector<SpriteInstance>& sprites() const { return m_sprites; }
    std::size_t size() const { return m_sprites.size(); }
    bool empty() const { return m_sprites.empty(); }

    // Retains capacity across frames so steady-state rendering does not
    // allocate.
    void reserve(std::size_t count) { m_sprites.reserve(count); }

private:
    std::vector<SpriteInstance> m_sprites;
};

} // namespace hu
