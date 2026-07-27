#pragma once

// Playback for a LevelDefinition.
//
// The director owns the level clock and decides *when* something spawns. It
// never owns the resulting enemies: each one is handed straight to a callback
// the scene supplies, so GameScene remains the only container of live enemies
// and the director stays trivially resettable and testable.

#include "Core/GameTypes.h"
#include "Gameplay/Enemies/EnemyBase.h"
#include "Gameplay/Levels/LevelDefinition.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace hu {

class LevelDirector {
public:
    using SpawnCallback = std::function<void(std::unique_ptr<EnemyBase>)>;

    LevelDirector();

    // --- Setup ------------------------------------------------------------
    // Enemies are handed over here as they spawn. Without a callback the
    // director still runs its clock but drops everything it creates.
    void setSpawnCallback(SpawnCallback callback);

    // Looks the level up in LevelRegistry. Returns false (and clears the
    // current level) when the id is unknown.
    bool loadLevel(const std::string& id);

    // Plays a definition that is not in the registry -- used by the editor for
    // preview. The definition must outlive the director.
    void setLevel(const LevelDefinition* level);

    void setDifficulty(DifficultyMode mode);
    DifficultyMode difficulty() const { return m_difficulty; }

    // Rewinds the clock and un-fires every wave. Keeps the level and callback.
    void reset();

    // --- Playback ---------------------------------------------------------
    void update(float dt);

    // The scene calls this when the boss enemy dies; the director cannot see
    // the live enemy list.
    void notifyBossDefeated();

    // --- Queries ----------------------------------------------------------
    const LevelDefinition* level() const { return m_level; }
    float elapsedTime() const { return m_elapsed; }
    float scrollSpeed() const;

    bool isRunning() const { return m_level != nullptr && !m_complete; }
    bool isComplete() const { return m_complete; }
    bool bossSpawned() const { return m_bossSpawned; }
    bool bossDefeated() const { return m_bossDefeated; }

    // True once the wave content is done and the fight is with the boss.
    bool inBossFight() const { return m_bossSpawned && !m_bossDefeated; }

    // 0..1 for the HUD. Wave content occupies the first 90%, the boss the rest.
    float progress01() const;

    // Name of the most recently started wave; empty before the first one.
    const std::string& currentWaveName() const { return m_currentWaveName; }

    std::size_t wavesFired() const { return m_wavesStarted; }
    std::size_t enemiesSpawned() const { return m_enemiesSpawned; }

private:
    void spawnRow(const Wave& wave, const WaveSpawn& row);
    void spawnBoss();

    const LevelDefinition* m_level = nullptr;
    SpawnCallback m_spawnCallback;
    DifficultyMode m_difficulty = DifficultyMode::Normal;

    float m_elapsed = 0.0f;
    bool m_complete = false;
    bool m_bossSpawned = false;
    bool m_bossDefeated = false;

    // Parallel to m_level->waves: has the wave's start been logged yet.
    std::vector<bool> m_waveStarted;
    // Flat, parallel to the concatenation of every wave's spawns.
    std::vector<bool> m_spawnFired;
    // Index of the first flat slot belonging to each wave.
    std::vector<std::size_t> m_waveSpawnOffset;

    std::string m_currentWaveName;
    std::size_t m_wavesStarted = 0;
    std::size_t m_enemiesSpawned = 0;
};

} // namespace hu
