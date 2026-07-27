#pragma once

// Canonical sprite table.
//
// This list is the contract between three things that must agree exactly:
//   * tools/generate_art.py, which bakes the atlas and writes assets/atlas.json
//   * the renderer, which resolves each name to a UV rect at load time
//   * gameplay code, which refers to sprites by enum
//
// Adding a sprite means adding it in all three places. The renderer logs a
// warning for any enum entry missing from the atlas, so a mismatch is loud
// rather than silently drawing the wrong art.

#include <cstddef>
#include <cstdint>

namespace hu {

enum class SpriteId : std::uint16_t {
    // Player
    ShipPlayer = 0,
    ShipPlayerBankUp,
    ShipPlayerBankDown,

    // Enemies
    EnemyDrifter,
    EnemyWaveRider,
    EnemyDiver,
    EnemyTurret,
    EnemySplitter,
    EnemySplitterChild,
    EnemyOrbiter,
    EnemyMine,
    EnemyBoss,

    // Player projectiles
    BulletPlayer,
    MissilePlayer,
    LancePlayer,
    LaserSegment,
    LaserHead,

    // Enemy projectiles
    BulletEnemy,
    BulletEnemyHeavy,
    MissileEnemy,

    // Power-ups
    PowerupSpread,
    PowerupMissile,
    PowerupLaser,
    PowerupCannon,
    PowerupRepair,
    PowerupEnergy,

    // Effects
    ParticleSpark,
    ParticleGlow,
    ParticleSmoke,
    ParticleRing,
    ParticleShard,

    // Background
    StarSmall,
    StarLarge,
    NebulaPatch,

    // Fallback: solid white texel, used for untextured/tinted quads and by the
    // HUD. Guaranteed present even if the atlas fails to load.
    White,

    Count
};

inline constexpr std::size_t SpriteIdCount = static_cast<std::size_t>(SpriteId::Count);

// Names must match the keys emitted into assets/atlas.json exactly.
inline const char* spriteName(SpriteId id) {
    switch (id) {
        case SpriteId::ShipPlayer:         return "ship_player";
        case SpriteId::ShipPlayerBankUp:   return "ship_player_bank_up";
        case SpriteId::ShipPlayerBankDown: return "ship_player_bank_down";

        case SpriteId::EnemyDrifter:       return "enemy_drifter";
        case SpriteId::EnemyWaveRider:     return "enemy_waverider";
        case SpriteId::EnemyDiver:         return "enemy_diver";
        case SpriteId::EnemyTurret:        return "enemy_turret";
        case SpriteId::EnemySplitter:      return "enemy_splitter";
        case SpriteId::EnemySplitterChild: return "enemy_splitter_child";
        case SpriteId::EnemyOrbiter:       return "enemy_orbiter";
        case SpriteId::EnemyMine:          return "enemy_mine";
        case SpriteId::EnemyBoss:          return "enemy_boss";

        case SpriteId::BulletPlayer:       return "bullet_player";
        case SpriteId::MissilePlayer:      return "missile_player";
        case SpriteId::LancePlayer:        return "lance_player";
        case SpriteId::LaserSegment:       return "laser_segment";
        case SpriteId::LaserHead:          return "laser_head";

        case SpriteId::BulletEnemy:        return "bullet_enemy";
        case SpriteId::BulletEnemyHeavy:   return "bullet_enemy_heavy";
        case SpriteId::MissileEnemy:       return "missile_enemy";

        case SpriteId::PowerupSpread:      return "powerup_spread";
        case SpriteId::PowerupMissile:     return "powerup_missile";
        case SpriteId::PowerupLaser:       return "powerup_laser";
        case SpriteId::PowerupCannon:      return "powerup_cannon";
        case SpriteId::PowerupRepair:      return "powerup_repair";
        case SpriteId::PowerupEnergy:      return "powerup_energy";

        case SpriteId::ParticleSpark:      return "particle_spark";
        case SpriteId::ParticleGlow:       return "particle_glow";
        case SpriteId::ParticleSmoke:      return "particle_smoke";
        case SpriteId::ParticleRing:       return "particle_ring";
        case SpriteId::ParticleShard:      return "particle_shard";

        case SpriteId::StarSmall:          return "star_small";
        case SpriteId::StarLarge:          return "star_large";
        case SpriteId::NebulaPatch:        return "nebula_patch";

        case SpriteId::White:              return "white";
        default:                           return "white";
    }
}

} // namespace hu
