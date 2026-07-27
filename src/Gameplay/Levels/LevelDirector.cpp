#include "Gameplay/Levels/LevelDirector.h"

#include "Core/Log.h"
#include "Core/Math.h"
#include "Gameplay/Enemies/EnemyFactory.h"
#include "Gameplay/Levels/Levels.h"

#include <utility>

namespace hu {

// ===========================================================================
// TUNABLES
// ===========================================================================
namespace {

constexpr const char* kLogCategory = "Level";

// Share of the progress bar given to the wave content; the remainder is the
// boss fight.
constexpr float kWaveProgressShare = 0.9f;

// Bullet Hell keeps the same wave layout but hardens what arrives -- the fire
// rate and bullet count changes live in the behaviours themselves.
constexpr float kBulletHellHealthScale = 1.25f;

} // namespace

LevelDirector::LevelDirector() = default;

void LevelDirector::setSpawnCallback(SpawnCallback callback) {
    m_spawnCallback = std::move(callback);
}

bool LevelDirector::loadLevel(const std::string& id) {
    const LevelDefinition* found = LevelRegistry::find(id);
    if (found == nullptr) {
        HU_LOG_ERROR(kLogCategory, "loadLevel: no level registered with id '%s'", id.c_str());
        setLevel(nullptr);
        return false;
    }
    setLevel(found);
    return true;
}

void LevelDirector::setLevel(const LevelDefinition* level) {
    m_level = level;

    m_waveStarted.clear();
    m_spawnFired.clear();
    m_waveSpawnOffset.clear();

    if (m_level != nullptr) {
        m_waveStarted.assign(m_level->waves.size(), false);
        m_waveSpawnOffset.reserve(m_level->waves.size());

        std::size_t offset = 0;
        for (const Wave& wave : m_level->waves) {
            m_waveSpawnOffset.push_back(offset);
            offset += wave.spawns.size();
        }
        m_spawnFired.assign(offset, false);

        HU_LOG_INFO(kLogCategory, "Level loaded: '%s' (%s) -- %zu waves, %zu spawns, %.0fs",
                    m_level->id.c_str(), m_level->displayName.c_str(),
                    m_level->waves.size(), offset, m_level->duration);
    }

    reset();
}

void LevelDirector::setDifficulty(DifficultyMode mode) {
    m_difficulty = mode;
}

void LevelDirector::reset() {
    m_elapsed = 0.0f;
    m_complete = false;
    m_bossSpawned = false;
    m_bossDefeated = false;
    m_currentWaveName.clear();
    m_wavesStarted = 0;
    m_enemiesSpawned = 0;

    m_waveStarted.assign(m_waveStarted.size(), false);
    m_spawnFired.assign(m_spawnFired.size(), false);
}

float LevelDirector::scrollSpeed() const {
    return m_level != nullptr ? m_level->scrollSpeed : LevelDefaultScrollSpeed;
}

void LevelDirector::update(float dt) {
    if (m_level == nullptr || m_complete) {
        return;
    }

    m_elapsed += dt;

    for (std::size_t w = 0; w < m_level->waves.size(); ++w) {
        const Wave& wave = m_level->waves[w];

        if (!m_waveStarted[w] && m_elapsed >= wave.startTime) {
            m_waveStarted[w] = true;
            ++m_wavesStarted;
            m_currentWaveName = wave.name;
            HU_LOG_INFO(kLogCategory, "Wave %zu start at %.1fs: '%s' (%zu spawns)", w + 1,
                        m_elapsed, wave.name.c_str(), wave.spawns.size());
        }
        if (!m_waveStarted[w]) {
            // Waves are authored in ascending start order, so nothing after this
            // one can be due yet.
            continue;
        }

        const std::size_t base = m_waveSpawnOffset[w];
        for (std::size_t s = 0; s < wave.spawns.size(); ++s) {
            const std::size_t flat = base + s;
            if (m_spawnFired[flat]) {
                continue;
            }
            const WaveSpawn& row = wave.spawns[s];
            if (m_elapsed >= absoluteSpawnTime(wave, row)) {
                m_spawnFired[flat] = true;
                spawnRow(wave, row);
            }
        }
    }

    if (!m_bossSpawned && m_elapsed >= m_level->duration) {
        if (m_level->hasBoss) {
            spawnBoss();
        } else {
            m_bossSpawned = true;
            m_bossDefeated = true;
        }
    }

    if (m_bossSpawned && m_bossDefeated && !m_complete) {
        m_complete = true;
        HU_LOG_INFO(kLogCategory,
                    "Level complete: '%s' in %.1fs -- %zu waves, %zu enemies spawned",
                    m_level->id.c_str(), m_elapsed, m_wavesStarted, m_enemiesSpawned);
    }
}

void LevelDirector::spawnRow(const Wave& wave, const WaveSpawn& row) {
    EnemySpawnParams params = toSpawnParams(row, m_level->scrollSpeed);
    if (m_difficulty == DifficultyMode::BulletHell) {
        params.healthScale *= kBulletHellHealthScale;
    }

    std::unique_ptr<EnemyBase> enemy = EnemyFactory::create(row.archetype, params);
    if (!enemy) {
        HU_LOG_WARN(kLogCategory, "Wave '%s': factory returned nothing for archetype %d",
                    wave.name.c_str(), static_cast<int>(row.archetype));
        return;
    }

    HU_LOG_DEBUG(kLogCategory, "[%.1fs] '%s' spawn %s at (%.0f, %.0f)", m_elapsed,
                 wave.name.c_str(), enemyArchetypeName(row.archetype),
                 params.position.x, params.position.y);

    ++m_enemiesSpawned;
    if (m_spawnCallback) {
        m_spawnCallback(std::move(enemy));
    }
}

void LevelDirector::spawnBoss() {
    m_bossSpawned = true;

    EnemySpawnParams params;
    params.position = { LevelBossSpawnX, LevelBossSpawnY };
    params.worldScrollSpeed = m_level->scrollSpeed;
    if (m_difficulty == DifficultyMode::BulletHell) {
        params.healthScale = kBulletHellHealthScale;
    }

    std::unique_ptr<EnemyBase> boss = EnemyFactory::create(EnemyArchetype::Boss, params);
    if (!boss) {
        HU_LOG_ERROR("Boss", "Boss spawn failed for level '%s'", m_level->id.c_str());
        m_bossDefeated = true;
        return;
    }

    HU_LOG_INFO("Boss", "Boss wave triggered at %.1fs in level '%s'", m_elapsed,
                m_level->id.c_str());

    ++m_enemiesSpawned;
    if (m_spawnCallback) {
        m_spawnCallback(std::move(boss));
    } else {
        // Nobody to own it: do not leave the level unwinnable.
        HU_LOG_WARN("Boss", "No spawn callback set; boss discarded");
        m_bossDefeated = true;
    }
}

void LevelDirector::notifyBossDefeated() {
    if (m_bossDefeated) {
        return;
    }
    m_bossDefeated = true;
    HU_LOG_INFO("Boss", "Boss defeated at %.1fs", m_elapsed);
}

float LevelDirector::progress01() const {
    if (m_level == nullptr) {
        return 0.0f;
    }
    if (m_complete) {
        return 1.0f;
    }

    const float duration = m_level->duration > 0.0f ? m_level->duration : 1.0f;
    const float wavePart = clampf(m_elapsed / duration, 0.0f, 1.0f) * kWaveProgressShare;
    if (!m_level->hasBoss) {
        return clampf(m_elapsed / duration, 0.0f, 1.0f);
    }
    if (!m_bossSpawned) {
        return wavePart;
    }
    // During the boss fight the bar creeps rather than jumping; the boss health
    // bar is the real readout at that point.
    return kWaveProgressShare;
}

} // namespace hu
