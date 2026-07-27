#pragma once

// Shared vocabulary for the whole game. Weapons, power-ups, superweapons and
// enemy archetypes are all named here so that gameplay, UI and rendering agree
// on a single set of identifiers.

#include <cstddef>
#include <cstdint>

namespace hu {

// ---------------------------------------------------------------------------
// Weapons
// ---------------------------------------------------------------------------

// Order matters: this is the cycle order for the [ and ] swap keys.
enum class WeaponType : std::uint8_t {
    Bullet = 0,  // Always available.
    Spread,      // Unlocked by the Spread power-up.
    Missile,     // Unlocked by the Missile power-up.
    Laser,       // Unlocked by the Laser power-up.
    Count
};

inline constexpr std::size_t WeaponTypeCount = static_cast<std::size_t>(WeaponType::Count);

// Every weapon tops out at level 5; further pick-ups of the same type are
// converted into score/energy by the power-up system.
inline constexpr int MaxWeaponLevel = 5;

inline const char* weaponName(WeaponType type) {
    switch (type) {
        case WeaponType::Bullet:  return "Bullet";
        case WeaponType::Spread:  return "Spread";
        case WeaponType::Missile: return "Missile";
        case WeaponType::Laser:   return "Laser";
        default:                  return "Unknown";
    }
}

// ---------------------------------------------------------------------------
// Superweapons
// ---------------------------------------------------------------------------

// Selected purely by how many energy cells are fully charged. Index equals the
// number of cells consumed.
enum class SuperweaponType : std::uint8_t {
    None = 0,
    PiercingLance = 1,   // 1 cell: Bullet + Missile.
    MissileBarrage = 2,  // 2 cells: Spread + Missile.
    LaserSpread = 3,     // 3 cells: Spread + Laser.
    HelixBeam = 4,       // 4 cells: Missile + Laser.
    EnergyBomb = 5       // 5 cells: everything at once.
};

inline const char* superweaponName(SuperweaponType type) {
    switch (type) {
        case SuperweaponType::PiercingLance:  return "Piercing Lance";
        case SuperweaponType::MissileBarrage: return "Missile Barrage";
        case SuperweaponType::LaserSpread:    return "Laser Spread";
        case SuperweaponType::HelixBeam:      return "Helix Beam";
        case SuperweaponType::EnergyBomb:     return "Energy Bomb";
        default:                              return "None";
    }
}

inline SuperweaponType superweaponForCharge(int chargedCells) {
    if (chargedCells >= 5) return SuperweaponType::EnergyBomb;
    if (chargedCells == 4) return SuperweaponType::HelixBeam;
    if (chargedCells == 3) return SuperweaponType::LaserSpread;
    if (chargedCells == 2) return SuperweaponType::MissileBarrage;
    if (chargedCells == 1) return SuperweaponType::PiercingLance;
    return SuperweaponType::None;
}

// ---------------------------------------------------------------------------
// Power-ups
// ---------------------------------------------------------------------------

enum class PowerupType : std::uint8_t {
    WeaponSpread = 0,   // Unlocks/upgrades the spread weapon.
    WeaponMissile,      // Unlocks/upgrades missiles.
    WeaponLaser,        // Unlocks/upgrades the laser.
    BulletUpgrade,      // Upgrades the default bullet weapon.
    CellRepair,         // Repairs the lowest broken energy cell.
    EnergyCharge,       // Adds charge to the lowest unfilled cell.
    Count
};

inline constexpr std::size_t PowerupTypeCount = static_cast<std::size_t>(PowerupType::Count);

inline const char* powerupName(PowerupType type) {
    switch (type) {
        case PowerupType::WeaponSpread:  return "Spread Module";
        case PowerupType::WeaponMissile: return "Missile Rack";
        case PowerupType::WeaponLaser:   return "Laser Core";
        case PowerupType::BulletUpgrade: return "Cannon Upgrade";
        case PowerupType::CellRepair:    return "Cell Repair";
        case PowerupType::EnergyCharge:  return "Energy Charge";
        default:                         return "Unknown";
    }
}

// Maps a weapon power-up to the weapon it affects. Returns WeaponType::Count
// for power-ups that are not weapon related.
inline WeaponType weaponForPowerup(PowerupType type) {
    switch (type) {
        case PowerupType::WeaponSpread:  return WeaponType::Spread;
        case PowerupType::WeaponMissile: return WeaponType::Missile;
        case PowerupType::WeaponLaser:   return WeaponType::Laser;
        case PowerupType::BulletUpgrade: return WeaponType::Bullet;
        default:                         return WeaponType::Count;
    }
}

// ---------------------------------------------------------------------------
// Factions and enemies
// ---------------------------------------------------------------------------

enum class Faction : std::uint8_t {
    Player = 0,
    Enemy
};

enum class EnemyArchetype : std::uint8_t {
    Drifter = 0,   // Straight-line fodder.
    WaveRider,     // Sine-wave sweep, spawns in formations.
    Diver,         // Closes on the player, attacks, then pulls back.
    Turret,        // Anchored to the scroll, heavy aimed fire.
    Splitter,      // Breaks into smaller enemies when destroyed.
    Orbiter,       // Circles a moving anchor point.
    Mine,          // Drifts slowly, detonates into a bullet ring.
    Boss,          // Multi-phase level boss.
    Count
};

inline constexpr std::size_t EnemyArchetypeCount = static_cast<std::size_t>(EnemyArchetype::Count);

inline const char* enemyArchetypeName(EnemyArchetype type) {
    switch (type) {
        case EnemyArchetype::Drifter:   return "Drifter";
        case EnemyArchetype::WaveRider: return "Wave Rider";
        case EnemyArchetype::Diver:     return "Diver";
        case EnemyArchetype::Turret:    return "Turret";
        case EnemyArchetype::Splitter:  return "Splitter";
        case EnemyArchetype::Orbiter:   return "Orbiter";
        case EnemyArchetype::Mine:      return "Mine";
        case EnemyArchetype::Boss:      return "Boss";
        default:                        return "Unknown";
    }
}

// ---------------------------------------------------------------------------
// High level game flow
// ---------------------------------------------------------------------------

enum class GameStateId : std::uint8_t {
    MainMenu = 0,
    Playing,
    Paused,
    LevelComplete,
    GameOver,
    Options,
    SecretsScreen
};

// Bullet hell mode is unlocked by finding every secret in every level; it
// multiplies enemy fire rates and bullet counts.
enum class DifficultyMode : std::uint8_t {
    Normal = 0,
    BulletHell
};

} // namespace hu
