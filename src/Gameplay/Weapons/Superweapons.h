#pragma once

// Superweapons: the payoff for the energy cell system.
//
// Either Shift fires one. The tier is chosen purely by how many cells are at
// 100% charge, and firing spends exactly those cells:
//
//   1 cell  Piercing Lance   Bullet + Missile
//   2 cells Missile Barrage  Spread + Missile
//   3 cells Laser Spread     Spread + Laser
//   4 cells Helix Beam       Missile + Laser
//   5 cells Energy Bomb      everything
//
// The two beam tiers persist for a couple of seconds, so this system keeps
// running after the trigger and publishes its live beam geometry for the
// scene's damage pass, exactly like WeaponSystem does for the basic laser.

#include "Core/DrawList.h"
#include "Core/GameTypes.h"
#include "Gameplay/IGameWorld.h"
#include "Game/Entity.h"

#include <vector>

namespace hu {

class WeaponSystem;
class EnergyCellSystem;

// One live superweapon beam. Runs from `origin` along `angle` for `length`
// pixels, `halfThickness` either side of the centre line.
struct BeamSegment {
    Vector2 origin{ 0.0f, 0.0f };
    float angle = 0.0f;
    float length = 0.0f;
    float halfThickness = 0.0f;
    float damagePerSecond = 0.0f;
};

class SuperweaponSystem {
public:
    void reset();

    void update(float deltaTime, Vector2 shipPosition, IGameWorld& world,
                WeaponSystem& weapons, EnergyCellSystem& cells);

    // Latches a fire request; it is serviced on the next update() so input and
    // simulation stay decoupled.
    void requestFire();

    void appendDraw(DrawList& out) const;

    // What would fire right now, given the current charge. None when no cell is
    // charged or a beam is already running.
    SuperweaponType pending(const EnergyCellSystem& cells) const;

    bool isActive() const { return m_active != SuperweaponType::None; }
    SuperweaponType activeType() const { return m_active; }
    float remainingTime() const { return m_timer; }

    // Live beam geometry for the scene's damage pass. Empty unless a beam tier
    // is active.
    const std::vector<BeamSegment>& beams() const { return m_beams; }

private:
    void fire(SuperweaponType type, int cells, Vector2 origin, IGameWorld& world,
              WeaponSystem& weapons);

    void firePiercingLance(Vector2 origin, IGameWorld& world);
    void fireMissileBarrage(Vector2 origin, IGameWorld& world);
    void beginLaserSpread(Vector2 origin, IGameWorld& world, WeaponSystem& weapons);
    void beginHelixBeam(Vector2 origin, IGameWorld& world);
    void fireEnergyBomb(Vector2 origin, IGameWorld& world);

    void updateBeams(float deltaTime, Vector2 origin, IGameWorld& world);

    SuperweaponType m_active = SuperweaponType::None;
    float m_timer = 0.0f;
    float m_duration = 0.0f;
    bool m_fireRequested = false;

    std::vector<BeamSegment> m_beams;
    std::vector<float> m_beamAngles;   // Fixed pattern; re-anchored each frame.
    float m_beamHalfThickness = 0.0f;
    float m_beamDamagePerSecond = 0.0f;
    float m_helixSpawnTimer = 0.0f;
    int m_helixEscortIndex = 0;
    float m_pulse = 0.0f;
};

} // namespace hu
