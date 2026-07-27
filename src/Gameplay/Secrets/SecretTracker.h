#pragma once

// Runtime evaluation of the secrets belonging to the active level.
//
// The tracker is entirely driven by gameplay events; it holds no references to
// gameplay objects and knows nothing about specific levels or secrets. Whatever
// SecretRegistry lists for the active level is evaluated, so new content needs
// no change here.

#include "Gameplay/Secrets/SecretDefinition.h"

#include <cstddef>
#include <string>
#include <vector>

namespace hu {

class SecretTracker {
public:
    // Per-secret runtime state. Exposed so the secrets UI can render progress
    // without duplicating the evaluation rules.
    struct Progress {
        const SecretDefinition* definition = nullptr;
        bool unlocked = false;
        bool failed = false;      // Permanently impossible for this run.
        int counter = 0;          // Kills so far / sequence position.
        int required = 0;         // Kills needed / sequence length (0 if n/a).
        float timerStart = -1.0f; // Level time the timed streak began (< 0 => idle).
    };

    // Resets all state and loads the definitions for the level.
    void onLevelStart(const std::string& levelId);

    // --- gameplay events ---------------------------------------------------
    void onPlayerMoved(const Vector2& position, float levelTime);
    void onEnemyDestroyed(EnemyArchetype archetype, const std::string& waveName, float levelTime);
    void onPlayerDamaged(float amount, float levelTime);
    void onPlayerFired(float levelTime);
    void onPowerupCollected(PowerupType type, float levelTime);
    void onWaveCleared(const std::string& waveName, float levelTime);
    void onBossDefeated(float levelTime);
    void update(float deltaTime, float levelTime);

    // Secrets unlocked since the last call. The caller (the scene) turns these
    // into a toast plus effect. Clears the pending list.
    std::vector<const SecretDefinition*> takeNewlyUnlocked();

    // --- queries -----------------------------------------------------------
    const std::string& activeLevelId() const { return m_levelId; }
    const std::vector<Progress>& progress() const { return m_progress; }
    bool isUnlocked(const std::string& secretId) const;
    std::size_t unlockedCount() const;
    std::size_t trackedCount() const { return m_progress.size(); }

private:
    void unlock(Progress& entry, float levelTime, const char* reason);
    void fail(Progress& entry, float levelTime, const char* reason);

    std::string m_levelId;
    std::vector<Progress> m_progress;
    std::vector<const SecretDefinition*> m_newlyUnlocked;

    // Derived timeline state used by the window/wave conditions.
    float m_levelTime = 0.0f;
    float m_lastDamageTime = -1.0f;   // < 0 => never damaged this run.
    float m_currentWaveStartTime = 0.0f;
};

} // namespace hu
