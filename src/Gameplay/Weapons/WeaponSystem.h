#pragma once

// The player's four primary weapons.
//
// Bullet is always available; Spread, Missile and Laser are unlocked by their
// matching power-up and levelled by repeats of it, capped at MaxWeaponLevel.
// [ and ] cycle through the unlocked weapons in WeaponType declaration order.
//
// Three of the four weapons spawn projectiles through IGameWorld. The Laser is
// different: it is a continuous swept beam that moves with the ship, so it is
// not a projectile at all. WeaponSystem draws it and publishes its geometry
// (origin, half-thickness, damage-per-second) for the scene's collision pass to
// sweep against enemies.

#include "Core/DrawList.h"
#include "Core/GameTypes.h"
#include "Gameplay/IGameWorld.h"
#include "Core/Vector2.h"

#include <cstddef>
#include <vector>

namespace hu {

class WeaponSystem {
public:
    WeaponSystem();

    // Back to "Bullet at level 1, everything else locked".
    void reset();

    void update(float deltaTime, Vector2 shipPosition, IGameWorld& world);

    // Trigger state; the laser fires continuously while held, the others on
    // their own cooldown.
    void setFiring(bool held);
    bool firing() const { return m_firing; }

    // -1 for '[', +1 for ']'. Locked weapons are skipped.
    void cycleWeapon(int direction);

    // Applies a weapon power-up: unlocks the weapon, or raises its level.
    void grantWeaponPowerup(PowerupType type);

    WeaponType current() const { return m_current; }
    int level(WeaponType t) const;
    bool unlocked(WeaponType t) const;

    void appendDraw(DrawList& out) const;

    // --- Laser damage region -----------------------------------------------
    // Valid only while laserActive(). The beam runs from laserOrigin() straight
    // right to the screen edge, laserHalfThickness() pixels either side of the
    // centre line, dealing laserDamagePerSecond() to everything it overlaps.
    bool laserActive() const { return m_laserActive; }
    float laserDamagePerSecond() const;
    float laserHalfThickness() const;
    Vector2 laserOrigin() const { return m_laserOrigin; }
    float laserLength() const { return m_laserLength; }

    // The spread pattern, in radians, for a given spread level. Defined once
    // here because the Laser Spread superweapon fires the same fan.
    static std::vector<float> spreadAngles(int level);

    // Thickness a level-5 laser reaches. The Helix Beam superweapon is
    // specified as twice this.
    static float maxLaserHalfThickness();

private:
    void fireBullet(Vector2 origin, IGameWorld& world);
    void fireSpread(Vector2 origin, IGameWorld& world);
    void fireMissile(Vector2 origin, IGameWorld& world);
    void updateLaser(float deltaTime, Vector2 origin, IGameWorld& world);

    // Muzzle position: the ship's nose, offset forward from its centre.
    static Vector2 muzzle(Vector2 shipPosition);

    WeaponType m_current = WeaponType::Bullet;
    int m_levels[WeaponTypeCount] = { 1, 0, 0, 0 };
    bool m_firing = false;
    float m_cooldown = 0.0f;

    // Laser state, refreshed every frame it is active.
    bool m_laserActive = false;
    Vector2 m_laserOrigin{ 0.0f, 0.0f };
    float m_laserLength = 0.0f;
    float m_laserPulse = 0.0f;   // Drives the animated core brightness.
};

} // namespace hu
