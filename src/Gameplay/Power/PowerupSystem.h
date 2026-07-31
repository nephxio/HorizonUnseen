#pragma once

// Power-up pickups and the enemy drop table.
//
// The scene calls maybeDropPowerup() whenever an enemy dies; this system rolls
// the weighted table and, on a hit, spawns a drifting pickup. Pickups scroll
// left with the level, bob gently, and are collected by proximity to the ship.
//
// Applying the effect is deliberately left to the scene: weapon power-ups go to
// WeaponSystem and cell power-ups go to EnergyCellSystem, and this system owns
// neither. It reports what was collected each frame instead.

#include "Core/DrawList.h"
#include "Core/GameTypes.h"
#include "Core/SpriteId.h"
#include "Gameplay/IGameWorld.h"
#include "Core/Vector2.h"

#include <cstddef>
#include <vector>

namespace hu {

struct Powerup {
    PowerupType type = PowerupType::EnergyCharge;
    Vector2 position{ 0.0f, 0.0f };
    Vector2 velocity{ 0.0f, 0.0f };
    float bobPhase = 0.0f;
    float age = 0.0f;
    float lifetime = 0.0f;
    bool alive = false;
};

class PowerupSystem {
public:
    void reset();

    // Rolls the drop table for a dead enemy. Returns true when something was
    // dropped. Tougher archetypes drop more often and are weighted toward the
    // better rewards; bullet-hell mode is more generous still.
    bool maybeDropPowerup(Vector2 position, EnemyArchetype archetype, DifficultyMode mode);

    // Unconditional spawn, for scripted drops and secrets.
    void spawn(PowerupType type, Vector2 position);

    // `scrollSpeed` is the level's leftward scroll in pixels/second.
    void update(float deltaTime, Vector2 playerPosition, float scrollSpeed, IGameWorld& world);

    void appendDraw(DrawList& out) const;

    // Everything picked up during the most recent update(). The scene drains
    // this to apply the effects.
    const std::vector<PowerupType>& collectedThisFrame() const { return m_collected; }

    const std::vector<Powerup>& pickups() const { return m_pickups; }
    std::size_t activeCount() const;

    static SpriteId spriteFor(PowerupType type);

private:
    static PowerupType rollType(EnemyArchetype archetype);

    std::vector<Powerup> m_pickups;
    std::vector<PowerupType> m_collected;
};

} // namespace hu
