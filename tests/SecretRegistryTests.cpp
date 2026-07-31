// Integrity checks over the shipped secret definitions.
//
// Unlike the tracker tests, these run against the real registry -- they are
// content checks rather than logic checks. Finding every secret in every level
// unlocks Bullet Hell, and the total is derived by walking the registries, so a
// secret that cannot physically be earned does not fail loudly. It just makes
// the unlock unreachable and nobody finds out until someone tries to 100% the
// game.
//
// Each structural check below corresponds to a guard in SecretTracker that
// silently skips a malformed condition.

#include "Gameplay/Levels/Levels.h"
#include "Gameplay/Secrets/SecretRegistry.h"

#include <catch2/catch_test_macros.hpp>

#include <set>
#include <string>

TEST_CASE("The registry is not empty", "[secrets][registry]") {
    CHECK_FALSE(hu::SecretRegistry::all().empty());
    CHECK(hu::SecretRegistry::totalCount() > 0);
}

TEST_CASE("Every secret id is unique", "[secrets][registry]") {
    // Save data keys off these ids, so a duplicate would make two secrets share
    // one unlock and quietly lower the bar for Bullet Hell.
    std::set<std::string> seen;
    for (const hu::SecretDefinition& definition : hu::SecretRegistry::all()) {
        INFO("duplicate secret id: " << definition.id);
        CHECK(seen.insert(definition.id).second);
    }
}

TEST_CASE("Every secret is fully populated", "[secrets][registry]") {
    for (const hu::SecretDefinition& definition : hu::SecretRegistry::all()) {
        INFO("secret: " << definition.id);
        CHECK_FALSE(definition.id.empty());
        CHECK_FALSE(definition.levelId.empty());
        CHECK_FALSE(definition.displayName.empty());
        CHECK_FALSE(definition.hint.empty());   // the secrets screen shows this

        // The registry builds ids as "<levelId>.<shortId>"; keeping that
        // convention is what makes an id readable in a save file.
        CHECK(definition.id.rfind(definition.levelId + ".", 0) == 0);
    }
}

TEST_CASE("Every secret belongs to a registered level", "[secrets][registry]") {
    // totalCount() walks LevelRegistry, so a secret attached to an unregistered
    // level id is excluded from the total by design -- which means a typo in a
    // levelId removes the secret from the game without any error.
    for (const hu::SecretDefinition& definition : hu::SecretRegistry::all()) {
        INFO("secret '" << definition.id << "' names level '" << definition.levelId << "'");
        CHECK(hu::LevelRegistry::find(definition.levelId) != nullptr);
    }
}

TEST_CASE("The total matches the sum over registered levels", "[secrets][registry]") {
    std::size_t sum = 0;
    for (const hu::LevelDefinition& level : hu::LevelRegistry::all()) {
        sum += hu::SecretRegistry::countForLevel(level.id);
    }

    CHECK(sum == hu::SecretRegistry::totalCount());

    // With every secret on a registered level (checked above), the total should
    // also account for the entire table.
    CHECK(hu::SecretRegistry::totalCount() == hu::SecretRegistry::all().size());
}

TEST_CASE("forLevel returns exactly that level's secrets", "[secrets][registry]") {
    for (const hu::LevelDefinition& level : hu::LevelRegistry::all()) {
        INFO("level: " << level.id);
        const std::vector<const hu::SecretDefinition*> secrets =
            hu::SecretRegistry::forLevel(level.id);

        CHECK(secrets.size() == hu::SecretRegistry::countForLevel(level.id));
        for (const hu::SecretDefinition* secret : secrets) {
            REQUIRE(secret != nullptr);
            CHECK(secret->levelId == level.id);
        }
    }
}

TEST_CASE("forLevel is empty for a level that does not exist", "[secrets][registry]") {
    CHECK(hu::SecretRegistry::forLevel("no_such_level").empty());
    CHECK(hu::SecretRegistry::countForLevel("no_such_level") == 0);
}

TEST_CASE("find round-trips every secret and rejects unknown ids", "[secrets][registry]") {
    for (const hu::SecretDefinition& definition : hu::SecretRegistry::all()) {
        INFO("secret: " << definition.id);
        const hu::SecretDefinition* found = hu::SecretRegistry::find(definition.id);
        REQUIRE(found != nullptr);
        CHECK(found->id == definition.id);
    }

    CHECK(hu::SecretRegistry::find("no_such_secret") == nullptr);
    CHECK(hu::SecretRegistry::find("") == nullptr);
}

TEST_CASE("No shipped secret is structurally unreachable", "[secrets][registry]") {
    // Every branch here mirrors a guard in SecretTracker that skips a condition
    // rather than reporting it, so a malformed definition is invisible at
    // runtime. These are the shapes that can never fire.
    for (const hu::SecretDefinition& definition : hu::SecretRegistry::all()) {
        const hu::SecretCondition& condition = definition.condition;
        INFO("secret: " << definition.id
             << " (" << hu::secretConditionKindName(condition.kind) << ")");

        switch (condition.kind) {
            case hu::SecretConditionKind::ReachLocation:
                // A region with neither a radius nor an extent contains only a
                // single exact point, which no player will ever land on.
                CHECK((condition.region.radius > 0.0f ||
                       (condition.region.width > 0.0f && condition.region.height > 0.0f)));
                break;

            case hu::SecretConditionKind::DestroyTargets:
                // The tracker only unlocks when `required > 0`.
                CHECK(condition.requiredCount > 0);
                CHECK(condition.timeLimit >= 0.0f);
                break;

            case hu::SecretConditionKind::SurviveWindow:
            case hu::SecretConditionKind::NoFireWindow:
                // These unlock from update() only when hasWindow() holds, so a
                // degenerate window means the secret never completes.
                CHECK(condition.hasWindow());
                break;

            case hu::SecretConditionKind::CollectSequence:
                // An empty sequence is skipped outright in onPowerupCollected.
                CHECK_FALSE(condition.sequence.empty());
                break;

            case hu::SecretConditionKind::DefeatBossUnder:
                // Unlock requires `levelTime < timeLimit`; a limit of zero can
                // never be beaten.
                CHECK(condition.timeLimit > 0.0f);
                break;

            case hu::SecretConditionKind::FlawlessWave:
                // Matched by name against onWaveCleared.
                CHECK_FALSE(condition.waveName.empty());
                break;

            default:
                FAIL("secret uses an unhandled condition kind");
                break;
        }
    }
}

TEST_CASE("Every secret describes itself", "[secrets][registry]") {
    for (const hu::SecretDefinition& definition : hu::SecretRegistry::all()) {
        INFO("secret: " << definition.id);
        const std::string text = hu::describeSecret(definition);

        CHECK_FALSE(text.empty());
        // "unknown condition" is describeSecret's fallback for a kind it does
        // not handle, so seeing it means the two switches have drifted apart.
        CHECK(text.find("unknown condition") == std::string::npos);
    }
}
