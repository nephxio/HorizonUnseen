#include "Gameplay/Secrets/SecretTracker.h"

#include "Core/Log.h"
#include "Gameplay/Secrets/SecretRegistry.h"

namespace hu {

namespace {

constexpr const char* kLogCategory = "Secrets";

} // namespace

// ---------------------------------------------------------------------------
// Lifetime
// ---------------------------------------------------------------------------

void SecretTracker::onLevelStart(const std::string& levelId) {
    m_levelId = levelId;
    m_progress.clear();
    m_newlyUnlocked.clear();
    m_levelTime = 0.0f;
    m_lastDamageTime = -1.0f;
    m_currentWaveStartTime = 0.0f;

    const std::vector<const SecretDefinition*> definitions = SecretRegistry::forLevel(levelId);
    m_progress.reserve(definitions.size());

    for (const SecretDefinition* definition : definitions) {
        Progress entry;
        entry.definition = definition;
        switch (definition->condition.kind) {
            case SecretConditionKind::DestroyTargets:
                entry.required = definition->condition.requiredCount;
                break;
            case SecretConditionKind::CollectSequence:
                entry.required = static_cast<int>(definition->condition.sequence.size());
                break;
            default:
                entry.required = 0;
                break;
        }
        m_progress.push_back(entry);
    }

    HU_LOG_INFO(kLogCategory, "Tracking %zu secret(s) for level '%s'",
                m_progress.size(), levelId.c_str());
}

void SecretTracker::unlock(Progress& entry, float levelTime, const char* reason) {
    if (entry.unlocked || entry.failed || entry.definition == nullptr) {
        return;
    }
    entry.unlocked = true;
    m_newlyUnlocked.push_back(entry.definition);
    HU_LOG_INFO(kLogCategory, "Secret unlocked: %s (%s) at t=%.2f - %s",
                entry.definition->id.c_str(),
                entry.definition->displayName.c_str(),
                levelTime,
                reason);
}

void SecretTracker::fail(Progress& entry, float levelTime, const char* reason) {
    if (entry.unlocked || entry.failed || entry.definition == nullptr) {
        return;
    }
    entry.failed = true;
    HU_LOG_DEBUG(kLogCategory, "Secret near miss: %s (%s) failed at t=%.2f - %s",
                 entry.definition->id.c_str(),
                 secretConditionKindName(entry.definition->condition.kind),
                 levelTime,
                 reason);
}

// ---------------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------------

void SecretTracker::onPlayerMoved(const Vector2& position, float levelTime) {
    for (Progress& entry : m_progress) {
        if (entry.unlocked || entry.failed) {
            continue;
        }
        const SecretCondition& condition = entry.definition->condition;
        if (condition.kind != SecretConditionKind::ReachLocation) {
            continue;
        }
        if (!condition.region.contains(position)) {
            continue;
        }
        if (!condition.inWindow(levelTime)) {
            HU_LOG_DEBUG(kLogCategory,
                         "Secret near miss: %s reached the region at t=%.2f, outside "
                         "the window [%.2f, %.2f]",
                         entry.definition->id.c_str(),
                         levelTime, condition.windowStart, condition.windowEnd);
            continue;
        }
        unlock(entry, levelTime, "region entered");
    }
}

void SecretTracker::onEnemyDestroyed(EnemyArchetype archetype,
                                     const std::string& waveName,
                                     float levelTime) {
    for (Progress& entry : m_progress) {
        if (entry.unlocked || entry.failed) {
            continue;
        }
        const SecretCondition& condition = entry.definition->condition;
        if (condition.kind != SecretConditionKind::DestroyTargets) {
            continue;
        }
        if (condition.archetype != archetype) {
            continue;
        }
        if (!condition.waveName.empty() && condition.waveName != waveName) {
            continue;
        }
        if (!condition.inWindow(levelTime)) {
            continue;
        }

        // A time limit turns this into a streak: the clock starts on the first
        // kill and the streak restarts once it lapses.
        if (condition.timeLimit > 0.0f) {
            if (entry.timerStart < 0.0f) {
                entry.timerStart = levelTime;
                entry.counter = 0;
            } else if ((levelTime - entry.timerStart) > condition.timeLimit) {
                HU_LOG_DEBUG(kLogCategory,
                             "Secret near miss: %s streak lapsed at t=%.2f with %d/%d "
                             "kills (limit %.2fs); restarting",
                             entry.definition->id.c_str(), levelTime,
                             entry.counter, entry.required, condition.timeLimit);
                entry.timerStart = levelTime;
                entry.counter = 0;
            }
        }

        ++entry.counter;
        if (entry.counter >= entry.required && entry.required > 0) {
            unlock(entry, levelTime, "target count reached");
        }
    }
}

void SecretTracker::onPlayerDamaged(float amount, float levelTime) {
    if (amount <= 0.0f) {
        return;
    }
    m_lastDamageTime = levelTime;

    for (Progress& entry : m_progress) {
        if (entry.unlocked || entry.failed) {
            continue;
        }
        const SecretCondition& condition = entry.definition->condition;
        if (condition.kind != SecretConditionKind::SurviveWindow) {
            continue;
        }
        if (condition.inWindow(levelTime)) {
            fail(entry, levelTime, "survive window: took damage inside the window");
        }
    }
}

void SecretTracker::onPlayerFired(float levelTime) {
    for (Progress& entry : m_progress) {
        if (entry.unlocked || entry.failed) {
            continue;
        }
        const SecretCondition& condition = entry.definition->condition;
        if (condition.kind != SecretConditionKind::NoFireWindow) {
            continue;
        }
        if (condition.inWindow(levelTime)) {
            fail(entry, levelTime, "no-fire window: shot fired inside the window");
        }
    }
}

void SecretTracker::onPowerupCollected(PowerupType type, float levelTime) {
    for (Progress& entry : m_progress) {
        if (entry.unlocked || entry.failed) {
            continue;
        }
        const SecretCondition& condition = entry.definition->condition;
        if (condition.kind != SecretConditionKind::CollectSequence) {
            continue;
        }
        if (condition.sequence.empty() || !condition.inWindow(levelTime)) {
            continue;
        }

        const std::size_t index = static_cast<std::size_t>(entry.counter);
        if (index < condition.sequence.size() && condition.sequence[index] == type) {
            ++entry.counter;
            if (static_cast<std::size_t>(entry.counter) >= condition.sequence.size()) {
                unlock(entry, levelTime, "sequence completed");
            }
        } else if (entry.counter > 0) {
            // Restart, but allow the wrong pick-up to become the new first step.
            const int previous = entry.counter;
            entry.counter = (condition.sequence[0] == type) ? 1 : 0;
            HU_LOG_DEBUG(kLogCategory,
                         "Secret near miss: %s sequence broken at step %d by %s at t=%.2f",
                         entry.definition->id.c_str(), previous, powerupName(type), levelTime);
        }
    }
}

void SecretTracker::onWaveCleared(const std::string& waveName, float levelTime) {
    for (Progress& entry : m_progress) {
        if (entry.unlocked || entry.failed) {
            continue;
        }
        const SecretCondition& condition = entry.definition->condition;
        if (condition.kind != SecretConditionKind::FlawlessWave) {
            continue;
        }
        if (condition.waveName != waveName) {
            continue;
        }
        // A wave runs from the moment the previous one was cleared (or level
        // start) until now, so "flawless" means no damage in that span.
        if (m_lastDamageTime >= m_currentWaveStartTime && m_lastDamageTime <= levelTime) {
            HU_LOG_DEBUG(kLogCategory,
                         "Secret near miss: %s flawless wave '%s' failed - took damage "
                         "at t=%.2f (wave span %.2f - %.2f)",
                         entry.definition->id.c_str(), waveName.c_str(),
                         m_lastDamageTime, m_currentWaveStartTime, levelTime);
            fail(entry, levelTime, "flawless wave: energy cell HP lost during the wave");
        } else {
            unlock(entry, levelTime, "wave cleared without damage");
        }
    }

    m_currentWaveStartTime = levelTime;
}

void SecretTracker::onBossDefeated(float levelTime) {
    for (Progress& entry : m_progress) {
        if (entry.unlocked || entry.failed) {
            continue;
        }
        const SecretCondition& condition = entry.definition->condition;
        if (condition.kind != SecretConditionKind::DefeatBossUnder) {
            continue;
        }
        if (levelTime < condition.timeLimit) {
            unlock(entry, levelTime, "boss defeated inside the time limit");
        } else {
            HU_LOG_DEBUG(kLogCategory,
                         "Secret near miss: %s boss defeated at t=%.2f, limit was %.2fs "
                         "(over by %.2fs)",
                         entry.definition->id.c_str(), levelTime, condition.timeLimit,
                         levelTime - condition.timeLimit);
            fail(entry, levelTime, "defeat boss under: time limit exceeded");
        }
    }
}

void SecretTracker::update(float deltaTime, float levelTime) {
    (void)deltaTime;
    m_levelTime = levelTime;

    for (Progress& entry : m_progress) {
        if (entry.unlocked || entry.failed) {
            continue;
        }
        const SecretCondition& condition = entry.definition->condition;

        switch (condition.kind) {
            case SecretConditionKind::SurviveWindow:
            case SecretConditionKind::NoFireWindow:
                // Nothing broke the streak while the window was open.
                if (condition.hasWindow() && levelTime > condition.windowEnd) {
                    unlock(entry, levelTime,
                           condition.kind == SecretConditionKind::SurviveWindow
                               ? "survived the window untouched"
                               : "passed the window without firing");
                }
                break;

            case SecretConditionKind::DestroyTargets:
                // Let a lapsed streak show up in the log even when no further
                // kills arrive, and clear it so progress reads honestly.
                if (condition.timeLimit > 0.0f && entry.timerStart >= 0.0f &&
                    (levelTime - entry.timerStart) > condition.timeLimit) {
                    HU_LOG_DEBUG(kLogCategory,
                                 "Secret near miss: %s streak expired at t=%.2f with %d/%d "
                                 "kills (limit %.2fs)",
                                 entry.definition->id.c_str(), levelTime,
                                 entry.counter, entry.required, condition.timeLimit);
                    entry.timerStart = -1.0f;
                    entry.counter = 0;
                }
                break;

            case SecretConditionKind::ReachLocation:
                if (condition.hasWindow() && levelTime > condition.windowEnd) {
                    fail(entry, levelTime, "reach location: window closed");
                }
                break;

            default:
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

std::vector<const SecretDefinition*> SecretTracker::takeNewlyUnlocked() {
    std::vector<const SecretDefinition*> result;
    result.swap(m_newlyUnlocked);
    return result;
}

bool SecretTracker::isUnlocked(const std::string& secretId) const {
    for (const Progress& entry : m_progress) {
        if (entry.definition != nullptr && entry.definition->id == secretId) {
            return entry.unlocked;
        }
    }
    return false;
}

std::size_t SecretTracker::unlockedCount() const {
    std::size_t count = 0;
    for (const Progress& entry : m_progress) {
        if (entry.unlocked) {
            ++count;
        }
    }
    return count;
}

} // namespace hu
