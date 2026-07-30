#pragma once

// Plain-data view models for the UI.
//
// The UI never reaches into gameplay objects. Each frame the scene fills in
// these structs and hands them to the widgets. That keeps ImGui code out of
// gameplay, lets menus be exercised without a running level, and means a
// gameplay refactor cannot ripple into the HUD.

#include "Core/GameTypes.h"
#include "Game/Entity.h"   // Vector2

#include <cstddef>
#include <string>
#include <vector>

namespace hu {

// One energy cell as the HUD needs to see it.
struct CellView {
    float health = 0.0f;
    float maxHealth = 1.0f;
    float charge = 0.0f;
    float maxCharge = 1.0f;
    bool broken = false;
    bool charged = false;
};

struct WeaponView {
    WeaponType type = WeaponType::Bullet;
    int level = 1;
    bool unlocked = false;
};

// A floating label drawn next to a power-up on the field.
//
// Icons alone do not survive being small, tinted and in motion -- players could
// see that something dropped but not what it was. The name is drawn in world
// space instead, which is unambiguous regardless of art.
struct PowerupLabel {
    Vector2 position{ 0.0f, 0.0f };   // World pixels; maps 1:1 to screen.
    std::string text;
    PowerupType type = PowerupType::EnergyCharge;
    float alpha = 1.0f;               // Follows the pickup's expiry blink.
};

struct HudModel {
    std::vector<CellView> cells;
    std::vector<WeaponView> weapons;   // Indexed by WeaponType order.
    WeaponType currentWeapon = WeaponType::Bullet;

    SuperweaponType pendingSuperweapon = SuperweaponType::None;
    int chargedCells = 0;

    // Damage pressure readout: the rolling 5-second rate that decides whether
    // incoming fire charges the cells or breaks them. Showing this makes the
    // core mechanic legible to the player instead of mysterious.
    float damageRate = 0.0f;
    float damageThreshold = 0.0f;

    float levelProgress01 = 0.0f;
    std::string levelName;
    long long score = 0;

    int secretsFound = 0;
    int secretsTotal = 0;

    // Near-misses this run. Grazing converts incoming fire into cell charge,
    // so this doubles as a readout of how much superweapon fuel the player is
    // earning by flying close.
    long long grazeCount = 0;

    // Power-ups currently on the field, labelled by name.
    std::vector<PowerupLabel> powerupLabels;

    bool bossActive = false;
    float bossHealth01 = 0.0f;
    std::string bossName;

    DifficultyMode difficulty = DifficultyMode::Normal;
};

// Transient on-screen notifications ("Secret found!", "Laser Core acquired").
enum class ToastKind {
    Info,
    Secret,
    Powerup,
    Warning
};

struct Toast {
    std::string title;
    std::string subtitle;
    ToastKind kind = ToastKind::Info;
    float age = 0.0f;
    float lifetime = 3.0f;
};

// A level's progress as shown on the secrets screen.
struct LevelProgressView {
    std::string levelId;
    std::string displayName;
    bool played = false;
    bool completed = false;
    int secretsFound = 0;
    int secretsTotal = 0;
    // Hints are revealed only once the level has been played at least once, so
    // the first run stays a discovery.
    std::vector<std::string> secretNames;
    std::vector<bool> secretUnlocked;
    std::vector<std::string> secretHints;
};

struct ProgressModel {
    std::vector<LevelProgressView> levels;
    int totalFound = 0;
    int totalSecrets = 0;
    bool bulletHellUnlocked = false;
    // True when the unlock came from the dev override rather than from actually
    // finding every secret, so the menu can label it honestly.
    bool devUnlockedBulletHell = false;
};

// What the menus ask the application to do. The menu widgets are pure: they
// return an action and never mutate game state themselves.
enum class MenuAction {
    None,
    StartNormal,
    StartBulletHell,
    OpenSecrets,
    OpenOptions,
    BackToMainMenu,
    Resume,
    RestartLevel,
    QuitToMenu,
    QuitGame
};

} // namespace hu
