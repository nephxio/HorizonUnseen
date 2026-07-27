#pragma once

// A single pooled projectile.
//
// Projectiles are never allocated individually: ProjectilePool owns fixed-size
// arrays of these and hands out slots. A Projectile is entirely described by
// the ProjectileSpawn it was initialised from, plus the small amount of state
// each motion mode needs (homing lock, orbit phase, pierce budget).

#include "Core/DrawList.h"
#include "Core/GameTypes.h"
#include "Gameplay/IGameWorld.h"
#include "Game/Entity.h"

#include <cstdint>

namespace hu {

class Projectile {
public:
    // Brings a pooled slot to life. Any previous state is fully overwritten.
    void initialize(const ProjectileSpawn& spawn, Faction faction);

    // Integrates motion, re-resolves homing locks, ages the projectile out and
    // culls it once it leaves the world bounds.
    void update(float deltaTime, IGameWorld& world);

    void appendDraw(DrawList& out) const;

    // Registers a confirmed hit. Returns true when the projectile was consumed
    // (and is therefore no longer alive). Piercing rounds survive until their
    // pierce budget is spent.
    bool onHit();

    void kill();

    bool alive() const { return m_alive; }
    Vector2 position() const { return m_position; }
    Vector2 velocity() const { return m_velocity; }
    float radius() const { return m_radius; }
    float damage() const { return m_damage; }
    Faction faction() const { return m_faction; }
    ProjectileMotion motion() const { return m_motion; }
    int pierceRemaining() const { return m_pierceRemaining; }
    bool emitsTrail() const { return m_emitsTrail; }
    SpriteId trailSprite() const { return m_trailSprite; }
    std::uint32_t targetHandle() const { return m_targetHandle; }

private:
    void updateHoming(float deltaTime, IGameWorld& world);
    void updateOrbiting(float deltaTime, IGameWorld& world);
    bool offWorld(const IGameWorld& world) const;

    Vector2 m_position{ 0.0f, 0.0f };
    Vector2 m_velocity{ 0.0f, 0.0f };
    Vector2 m_size{ 12.0f, 6.0f };
    Color m_tint{};
    SpriteId m_sprite = SpriteId::BulletPlayer;
    SpriteId m_trailSprite = SpriteId::ParticleSpark;
    ProjectileMotion m_motion = ProjectileMotion::Straight;
    Faction m_faction = Faction::Player;

    float m_damage = 10.0f;
    float m_radius = 4.0f;
    float m_lifetime = 5.0f;
    float m_age = 0.0f;
    float m_turnRate = 4.0f;
    float m_acceleration = 0.0f;
    float m_maxSpeed = 900.0f;
    float m_rotation = 0.0f;

    int m_pierceRemaining = 1;
    bool m_additive = false;
    bool m_emitsTrail = false;
    bool m_alive = false;

    // Homing
    std::uint32_t m_targetHandle = InvalidTarget;
    float m_reacquireTimer = 0.0f;

    // Orbiting (Helix Beam escorts): an anchor point slides forward along the
    // beam while the missile swings across it, then peels off to seek.
    Vector2 m_orbitAnchor{ 0.0f, 0.0f };
    Vector2 m_orbitAxis{ 1.0f, 0.0f };
    float m_orbitPhase = 0.0f;
    float m_orbitSpeed = 0.0f;
};

} // namespace hu
