#pragma once

// Persistent player progression.
//
// Stores which secrets have been found (by string id, never by index, so that
// reordering or deleting a secret can never silently change what a save means)
// and a per-level record of play/completion/best results.
//
// Bullet hell unlock is *derived*, never stored: it is true only when every
// secret of every registered level is present in the unlocked set. Adding a
// level or a secret therefore changes the requirement automatically, and an old
// save can never claim an unlock it no longer qualifies for.

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace hu {

struct LevelProgress {
    bool played = false;
    bool completed = false;
    float bestTimeSeconds = 0.0f;  // 0 => no completion recorded yet.
    std::int32_t bestScore = 0;
};

class SaveGame {
public:
    static constexpr const char* kDefaultPath = "saves/progress.dat";

    // Process-wide profile. Callers may also create their own instance (tests).
    static SaveGame& instance();

    // Reads the save file. Missing, corrupt or newer-versioned files are not
    // errors: the profile falls back to defaults and a warning is logged.
    // Returns true only when an existing file was read successfully.
    bool load(const std::string& path = kDefaultPath);

    // Writes the save file, creating the directory if needed.
    bool save(const std::string& path = kDefaultPath) const;

    // --- secrets -----------------------------------------------------------
    // Returns true if this call actually changed anything.
    bool unlockSecret(const std::string& secretId);
    bool isSecretUnlocked(const std::string& secretId) const;
    std::size_t secretsFoundInLevel(const std::string& levelId) const;
    std::size_t secretsTotalInLevel(const std::string& levelId) const;
    std::size_t secretsFoundTotal() const;
    std::size_t secretsTotal() const;
    const std::set<std::string>& unlockedSecrets() const { return m_unlockedSecrets; }

    // True only when every secret of every registered level is unlocked.
    // Cached, but recomputed on load, on unlock and on reset.
    bool bulletHellUnlocked() const { return m_bulletHellUnlocked; }

    // --- levels ------------------------------------------------------------
    void markLevelPlayed(const std::string& levelId);
    // Records a completion; best time/score are kept only when they improve.
    void markLevelCompleted(const std::string& levelId, float timeSeconds, std::int32_t score);
    const LevelProgress* levelProgress(const std::string& levelId) const;
    const std::map<std::string, LevelProgress>& allLevelProgress() const { return m_levels; }

    // --- audio settings ----------------------------------------------------
    // Stored in the profile rather than a separate settings file: there is only
    // one profile, and a second file would be a second thing to keep in step.
    // All three are clamped to [0, 1] on the way in.
    float masterVolume() const { return m_masterVolume; }
    float sfxVolume() const { return m_sfxVolume; }
    float musicVolume() const { return m_musicVolume; }
    void setMasterVolume(float volume);
    void setSfxVolume(float volume);
    void setMusicVolume(float volume);

    // --- lifecycle ---------------------------------------------------------
    void resetProgress();
    const std::string& lastError() const { return m_lastError; }

private:
    static constexpr std::uint32_t kMagic = 0x48555347;  // "HUSG" (Horizon Unseen Save Game)

    // v2 appended the three audio volumes. v1 files still load; they simply
    // keep the defaults below and gain the field on the next save.
    static constexpr std::uint32_t kVersion = 2;

    static constexpr float kDefaultMasterVolume = 0.8f;
    static constexpr float kDefaultSfxVolume = 1.0f;
    static constexpr float kDefaultMusicVolume = 0.6f;

    // Sanity ceilings so a corrupt length field cannot trigger a huge alloc.
    static constexpr std::uint32_t kMaxRecords = 100000;
    static constexpr std::uint32_t kMaxStringLength = 4096;

    void recomputeBulletHell();

    std::set<std::string> m_unlockedSecrets;
    std::map<std::string, LevelProgress> m_levels;
    bool m_bulletHellUnlocked = false;
    float m_masterVolume = kDefaultMasterVolume;
    float m_sfxVolume = kDefaultSfxVolume;
    float m_musicVolume = kDefaultMusicVolume;
    mutable std::string m_lastError;
};

} // namespace hu
