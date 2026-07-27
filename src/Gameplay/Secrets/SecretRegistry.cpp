#include "Gameplay/Secrets/SecretRegistry.h"

#include "Core/Log.h"

namespace hu {

namespace {

constexpr const char* kLogCategory = "Secrets";

SecretDefinition makeSecret(const std::string& levelId,
                            const std::string& shortId,
                            const std::string& displayName,
                            const std::string& hint,
                            const SecretCondition& condition) {
    SecretDefinition definition;
    definition.id = levelId + "." + shortId;
    definition.levelId = levelId;
    definition.displayName = displayName;
    definition.hint = hint;
    definition.condition = condition;
    return definition;
}

std::vector<SecretDefinition> buildDefinitions() {
    std::vector<SecretDefinition> definitions;

    // -----------------------------------------------------------------------
    // test_level
    // -----------------------------------------------------------------------
    const std::string testLevel = "test_level";

    definitions.push_back(makeSecret(
        testLevel, "hidden_alcove", "Hidden Alcove",
        "Something glints high on the right early in the run.",
        makeReachLocation(makeCircleRegion(1180.0f, 140.0f, 90.0f), 10.0f, 30.0f)));

    definitions.push_back(makeSecret(
        testLevel, "turret_purge", "Turret Purge",
        "The gun line can be broken faster than it can reload.",
        makeDestroyTargets(EnemyArchetype::Turret, 4, 10.0f, "vanguard")));

    definitions.push_back(makeSecret(
        testLevel, "silent_run", "Silent Run",
        "The mine field reacts to muzzle flash. Drift through it.",
        makeNoFireWindow(42.0f, 58.0f)));

    definitions.push_back(makeSecret(
        testLevel, "power_ritual", "Power Ritual",
        "Spread, then missiles, then laser. Order matters.",
        makeCollectSequence({PowerupType::WeaponSpread,
                             PowerupType::WeaponMissile,
                             PowerupType::WeaponLaser})));

    definitions.push_back(makeSecret(
        testLevel, "flawless_vanguard", "Flawless Vanguard",
        "Clear the opening wave without a scratch on your cells.",
        makeFlawlessWave("vanguard")));

    definitions.push_back(makeSecret(
        testLevel, "swift_execution", "Swift Execution",
        "The boss has a soft phase early. End it before it hardens.",
        makeDefeatBossUnder(150.0f)));

    return definitions;
}

} // namespace

const std::vector<SecretDefinition>& SecretRegistry::all() {
    static const std::vector<SecretDefinition> s_definitions = buildDefinitions();
    return s_definitions;
}

std::vector<const SecretDefinition*> SecretRegistry::forLevel(const std::string& levelId) {
    std::vector<const SecretDefinition*> result;
    for (const SecretDefinition& definition : all()) {
        if (definition.levelId == levelId) {
            result.push_back(&definition);
        }
    }
    return result;
}

std::size_t SecretRegistry::countForLevel(const std::string& levelId) {
    std::size_t count = 0;
    for (const SecretDefinition& definition : all()) {
        if (definition.levelId == levelId) {
            ++count;
        }
    }
    return count;
}

std::size_t SecretRegistry::totalCount() {
    std::size_t total = 0;
    for (const LevelDefinition& level : LevelRegistry::all()) {
        total += countForLevel(level.id);
    }

    // Cheap authoring guard: a secret pointing at an unregistered level would
    // otherwise silently never count towards bullet hell.
    if (total < all().size()) {
        HU_LOG_WARN(kLogCategory,
                    "%zu secret(s) are attached to level ids that are not registered "
                    "(registered total %zu, defined total %zu)",
                    all().size() - total, total, all().size());
    }
    return total;
}

const SecretDefinition* SecretRegistry::find(const std::string& secretId) {
    for (const SecretDefinition& definition : all()) {
        if (definition.id == secretId) {
            return &definition;
        }
    }
    return nullptr;
}

} // namespace hu
