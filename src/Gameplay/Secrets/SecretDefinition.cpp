#include "Gameplay/Secrets/SecretDefinition.h"

#include <cmath>

namespace hu {

const char* secretConditionKindName(SecretConditionKind kind) {
    switch (kind) {
        case SecretConditionKind::ReachLocation:   return "ReachLocation";
        case SecretConditionKind::DestroyTargets:  return "DestroyTargets";
        case SecretConditionKind::SurviveWindow:   return "SurviveWindow";
        case SecretConditionKind::NoFireWindow:    return "NoFireWindow";
        case SecretConditionKind::CollectSequence: return "CollectSequence";
        case SecretConditionKind::DefeatBossUnder: return "DefeatBossUnder";
        case SecretConditionKind::FlawlessWave:    return "FlawlessWave";
        default:                                   return "Unknown";
    }
}

// ---------------------------------------------------------------------------
// Region
// ---------------------------------------------------------------------------

bool SecretRegion::contains(const Vector2& point) const {
    if (radius > 0.0f) {
        const float dx = point.x - x;
        const float dy = point.y - y;
        return (dx * dx + dy * dy) <= (radius * radius);
    }
    return point.x >= x && point.x <= (x + width) &&
           point.y >= y && point.y <= (y + height);
}

SecretRegion makeCircleRegion(float centreX, float centreY, float radius) {
    SecretRegion region;
    region.x = centreX;
    region.y = centreY;
    region.radius = radius;
    return region;
}

SecretRegion makeRectRegion(float x, float y, float width, float height) {
    SecretRegion region;
    region.x = x;
    region.y = y;
    region.width = width;
    region.height = height;
    region.radius = 0.0f;
    return region;
}

// ---------------------------------------------------------------------------
// Condition factories
// ---------------------------------------------------------------------------

SecretCondition makeReachLocation(const SecretRegion& region, float windowStart, float windowEnd) {
    SecretCondition condition;
    condition.kind = SecretConditionKind::ReachLocation;
    condition.region = region;
    condition.windowStart = windowStart;
    condition.windowEnd = windowEnd;
    return condition;
}

SecretCondition makeDestroyTargets(EnemyArchetype archetype,
                                   int count,
                                   float timeLimit,
                                   const std::string& waveName) {
    SecretCondition condition;
    condition.kind = SecretConditionKind::DestroyTargets;
    condition.archetype = archetype;
    condition.requiredCount = count;
    condition.timeLimit = timeLimit;
    condition.waveName = waveName;
    return condition;
}

SecretCondition makeSurviveWindow(float windowStart, float windowEnd) {
    SecretCondition condition;
    condition.kind = SecretConditionKind::SurviveWindow;
    condition.windowStart = windowStart;
    condition.windowEnd = windowEnd;
    return condition;
}

SecretCondition makeNoFireWindow(float windowStart, float windowEnd) {
    SecretCondition condition;
    condition.kind = SecretConditionKind::NoFireWindow;
    condition.windowStart = windowStart;
    condition.windowEnd = windowEnd;
    return condition;
}

SecretCondition makeCollectSequence(const std::vector<PowerupType>& sequence) {
    SecretCondition condition;
    condition.kind = SecretConditionKind::CollectSequence;
    condition.sequence = sequence;
    return condition;
}

SecretCondition makeDefeatBossUnder(float seconds) {
    SecretCondition condition;
    condition.kind = SecretConditionKind::DefeatBossUnder;
    condition.timeLimit = seconds;
    return condition;
}

SecretCondition makeFlawlessWave(const std::string& waveName) {
    SecretCondition condition;
    condition.kind = SecretConditionKind::FlawlessWave;
    condition.waveName = waveName;
    return condition;
}

// ---------------------------------------------------------------------------
// Description
// ---------------------------------------------------------------------------

namespace {

std::string trimmedFloat(float value) {
    std::string text = std::to_string(value);
    while (text.size() > 1 && text.back() == '0') {
        text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
        text.pop_back();
    }
    return text;
}

} // namespace

std::string describeSecret(const SecretDefinition& definition) {
    const SecretCondition& condition = definition.condition;

    std::string text = definition.displayName;
    text += " [";
    text += secretConditionKindName(condition.kind);
    text += "] ";

    switch (condition.kind) {
        case SecretConditionKind::ReachLocation:
            if (condition.region.radius > 0.0f) {
                text += "reach (" + trimmedFloat(condition.region.x) + ", " +
                        trimmedFloat(condition.region.y) + ") within " +
                        trimmedFloat(condition.region.radius) + " units";
            } else {
                text += "reach rect at (" + trimmedFloat(condition.region.x) + ", " +
                        trimmedFloat(condition.region.y) + ")";
            }
            break;
        case SecretConditionKind::DestroyTargets:
            text += "destroy " + std::to_string(condition.requiredCount) + "x " +
                    enemyArchetypeName(condition.archetype);
            if (condition.timeLimit > 0.0f) {
                text += " within " + trimmedFloat(condition.timeLimit) + "s";
            }
            if (!condition.waveName.empty()) {
                text += " during wave '" + condition.waveName + "'";
            }
            break;
        case SecretConditionKind::SurviveWindow:
            text += "take no damage from " + trimmedFloat(condition.windowStart) +
                    "s to " + trimmedFloat(condition.windowEnd) + "s";
            break;
        case SecretConditionKind::NoFireWindow:
            text += "fire no shots from " + trimmedFloat(condition.windowStart) +
                    "s to " + trimmedFloat(condition.windowEnd) + "s";
            break;
        case SecretConditionKind::CollectSequence: {
            text += "collect in order:";
            for (std::size_t i = 0; i < condition.sequence.size(); ++i) {
                text += (i == 0 ? " " : " -> ");
                text += powerupName(condition.sequence[i]);
            }
            break;
        }
        case SecretConditionKind::DefeatBossUnder:
            text += "defeat the boss under " + trimmedFloat(condition.timeLimit) + "s";
            break;
        case SecretConditionKind::FlawlessWave:
            text += "clear wave '" + condition.waveName + "' without losing cell HP";
            break;
        default:
            text += "unknown condition";
            break;
    }

    if (condition.hasWindow() &&
        condition.kind != SecretConditionKind::SurviveWindow &&
        condition.kind != SecretConditionKind::NoFireWindow) {
        text += " (window " + trimmedFloat(condition.windowStart) + "s - " +
                trimmedFloat(condition.windowEnd) + "s)";
    }

    return text;
}

} // namespace hu
