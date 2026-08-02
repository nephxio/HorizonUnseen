#pragma once

// Canonical sound table.
//
// The same shape as SpriteId.h, and the same three-way contract:
//   * tools/generate_audio.py, which synthesises the wavs and writes
//     assets/sounds.json
//   * the audio engine, which resolves each name to a buffer at load time
//   * gameplay code, which refers to sounds by enum
//
// tools/check_audio.py fails the build when the enum and the manifest disagree,
// so a missing sound is caught in CI rather than discovered as silence.
//
// One-shots and music are separate enums because they are played differently:
// a SoundId is fired and forgotten on a pooled voice, a MusicId is streamed on
// a dedicated looping source with its own gain. Keeping them apart means
// playSound() cannot be handed a four-minute track by accident.

#include <cstddef>
#include <cstdint>

namespace hu {

enum class SoundId : std::uint16_t {
    // Weapons
    WeaponFire = 0,
    EnemyFire,

    // Impacts and destruction
    Impact,
    Explosion,
    BigExplosion,
    Debris,

    // Energy cells and pick-ups
    PowerupPickup,
    CellCharge,
    CellBreak,
    SuperweaponCharge,
    ScreenClear,

    // Signature mechanics and notifications
    Graze,
    SecretFound,

    // Menus
    UiMove,
    UiSelect,
    UiBack,

    Count
};

enum class MusicId : std::uint16_t {
    Menu = 0,
    Level,
    Boss,

    Count
};

inline constexpr std::size_t SoundIdCount = static_cast<std::size_t>(SoundId::Count);
inline constexpr std::size_t MusicIdCount = static_cast<std::size_t>(MusicId::Count);

// Names must match the keys emitted into assets/sounds.json exactly.
inline const char* soundName(SoundId id) {
    switch (id) {
        case SoundId::WeaponFire:        return "weapon_fire";
        case SoundId::EnemyFire:         return "enemy_fire";

        case SoundId::Impact:            return "impact";
        case SoundId::Explosion:         return "explosion";
        case SoundId::BigExplosion:      return "big_explosion";
        case SoundId::Debris:            return "debris";

        case SoundId::PowerupPickup:     return "powerup_pickup";
        case SoundId::CellCharge:        return "cell_charge";
        case SoundId::CellBreak:         return "cell_break";
        case SoundId::SuperweaponCharge: return "superweapon_charge";
        case SoundId::ScreenClear:       return "screen_clear";

        case SoundId::Graze:             return "graze";
        case SoundId::SecretFound:       return "secret_found";

        case SoundId::UiMove:            return "ui_move";
        case SoundId::UiSelect:          return "ui_select";
        case SoundId::UiBack:            return "ui_back";

        default:                         return "impact";
    }
}

// One queued one-shot. Gameplay produces these; the application drains them
// into the audio engine, exactly as it drains a DrawList into the renderer.
// Plain data, so the gameplay library never needs an audio header.
struct SoundEvent {
    SoundId id = SoundId::Impact;
    float gain = 1.0f;

    // -1 hard left, 0 centred, +1 hard right. Derived from where the event
    // happened on screen, so a turret dying on the right is heard on the right.
    float pan = 0.0f;
};

inline const char* musicName(MusicId id) {
    switch (id) {
        case MusicId::Menu:  return "music_menu";
        case MusicId::Level: return "music_level";
        case MusicId::Boss:  return "music_boss";
        default:             return "music_level";
    }
}

} // namespace hu
