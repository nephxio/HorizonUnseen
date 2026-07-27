#pragma once

// Declarative secret definitions.
//
// A secret is a named unlock condition attached to a level. Everything about a
// secret is data: the registry owns the definitions, the tracker evaluates them
// generically and the save system stores them by string id. Adding a secret (or
// a whole level of secrets) therefore never requires touching the tracker, the
// save file layout or the UI.

#include "Core/GameTypes.h"
#include "Game/Entity.h"

#include <cstdint>
#include <string>
#include <vector>

namespace hu {

// ---------------------------------------------------------------------------
// Condition kinds
// ---------------------------------------------------------------------------

enum class SecretConditionKind : std::uint8_t {
    ReachLocation = 0,  // Enter a region, optionally only inside a time window.
    DestroyTargets,     // Destroy N enemies of an archetype, optionally timed.
    SurviveWindow,      // Take no damage across a level-time window.
    NoFireWindow,       // Fire no shots across a level-time window.
    CollectSequence,    // Collect an ordered sequence of power-up types.
    DefeatBossUnder,    // Defeat the boss before a level time.
    FlawlessWave,       // Clear a named wave without losing energy cell HP.
    Count
};

const char* secretConditionKindName(SecretConditionKind kind);

// ---------------------------------------------------------------------------
// Region
// ---------------------------------------------------------------------------

// A circle when radius > 0, otherwise the axis-aligned rect at (x, y) with the
// given width/height.
struct SecretRegion {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float radius = 0.0f;

    bool contains(const Vector2& point) const;
};

SecretRegion makeCircleRegion(float centreX, float centreY, float radius);
SecretRegion makeRectRegion(float x, float y, float width, float height);

// ---------------------------------------------------------------------------
// Condition
// ---------------------------------------------------------------------------

// Tagged-union style: `kind` selects which of the members below are meaningful.
// Kept as a plain struct (rather than std::variant) so that the tracker can
// switch over it without visitor boilerplate and so new fields can be added
// without breaking existing call sites.
struct SecretCondition {
    SecretConditionKind kind = SecretConditionKind::ReachLocation;

    // Optional level-time window. windowEnd <= windowStart means "no window".
    float windowStart = 0.0f;
    float windowEnd = 0.0f;

    // ReachLocation
    SecretRegion region{};

    // DestroyTargets
    EnemyArchetype archetype = EnemyArchetype::Drifter;
    int requiredCount = 0;
    float timeLimit = 0.0f;   // 0 => untimed. Also used by DefeatBossUnder.

    // DestroyTargets (optional wave restriction) and FlawlessWave.
    std::string waveName;

    // CollectSequence
    std::vector<PowerupType> sequence;

    bool hasWindow() const { return windowEnd > windowStart; }
    bool inWindow(float levelTime) const {
        return !hasWindow() || (levelTime >= windowStart && levelTime <= windowEnd);
    }
};

// Condition factories. These exist so definition tables read as data rather than
// as a wall of designated initialisers.
SecretCondition makeReachLocation(const SecretRegion& region,
                                  float windowStart = 0.0f,
                                  float windowEnd = 0.0f);
SecretCondition makeDestroyTargets(EnemyArchetype archetype,
                                   int count,
                                   float timeLimit = 0.0f,
                                   const std::string& waveName = std::string());
SecretCondition makeSurviveWindow(float windowStart, float windowEnd);
SecretCondition makeNoFireWindow(float windowStart, float windowEnd);
SecretCondition makeCollectSequence(const std::vector<PowerupType>& sequence);
SecretCondition makeDefeatBossUnder(float seconds);
SecretCondition makeFlawlessWave(const std::string& waveName);

// ---------------------------------------------------------------------------
// Definition
// ---------------------------------------------------------------------------

struct SecretDefinition {
    std::string id;             // Globally unique, e.g. "test_level.hidden_alcove".
    std::string levelId;
    std::string displayName;
    std::string hint;           // Shown in the secrets UI once the level is played.
    SecretCondition condition;
};

// Human readable one-liner used by logging and the secrets screen.
std::string describeSecret(const SecretDefinition& definition);

} // namespace hu
