// Tests for SecretTracker's evaluation of each condition kind.
//
// The tracker is the only thing standing between a correctly authored secret
// and an unreachable one, and nothing else in the build exercises it: the
// headless episode test plays the game but never asserts that a specific secret
// fired for a specific reason. Finding every secret in every level is what
// unlocks Bullet Hell, so a rule that quietly stops firing gates content with
// no visible symptom.
//
// Definitions are built inline and handed to the tracker rather than read from
// SecretRegistry, so these tests describe the *rules* and are unaffected by
// whatever the shipped levels happen to declare.

#include "Gameplay/Secrets/SecretTracker.h"

#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <vector>

namespace {

constexpr const char* kLevel = "test_secrets";

// Owns a definition and hands the tracker a pointer to it. The tracker stores
// raw pointers and expects them to outlive it, which holds for the registry's
// statics and has to be arranged deliberately here.
class Fixture {
public:
    explicit Fixture(const hu::SecretCondition& condition, const std::string& id = "s.one") {
        add(condition, id);
        start();
    }

    Fixture() = default;

    void add(const hu::SecretCondition& condition, const std::string& id) {
        auto definition = std::make_unique<hu::SecretDefinition>();
        definition->id = id;
        definition->levelId = kLevel;
        definition->displayName = id;
        definition->condition = condition;
        m_pointers.push_back(definition.get());
        m_owned.push_back(std::move(definition));
    }

    void start() { m_tracker.onLevelStart(kLevel, m_pointers); }

    hu::SecretTracker& tracker() { return m_tracker; }

    // The single secret's progress entry, for tests that only registered one.
    const hu::SecretTracker::Progress& only() const {
        REQUIRE(m_tracker.progress().size() == 1);
        return m_tracker.progress().front();
    }

    bool unlocked() const { return only().unlocked; }
    bool failed() const { return only().failed; }
    int counter() const { return only().counter; }

private:
    std::vector<std::unique_ptr<hu::SecretDefinition>> m_owned;
    std::vector<const hu::SecretDefinition*> m_pointers;
    hu::SecretTracker m_tracker;
};

} // namespace

// ---------------------------------------------------------------------------
// ReachLocation
// ---------------------------------------------------------------------------

TEST_CASE("ReachLocation unlocks when the player enters the region", "[secrets][tracker]") {
    Fixture f{ hu::makeReachLocation(hu::makeCircleRegion(500.0f, 300.0f, 50.0f)) };

    f.tracker().onPlayerMoved(Vector2{ 0.0f, 0.0f }, 1.0f);
    CHECK_FALSE(f.unlocked());

    f.tracker().onPlayerMoved(Vector2{ 500.0f, 300.0f }, 2.0f);
    CHECK(f.unlocked());
}

TEST_CASE("ReachLocation ignores the region outside its window", "[secrets][tracker]") {
    Fixture f{ hu::makeReachLocation(hu::makeCircleRegion(500.0f, 300.0f, 50.0f), 10.0f, 30.0f) };

    // Right place, too early. This is a miss, not a failure -- the window has
    // not closed yet, so the player can still come back.
    f.tracker().onPlayerMoved(Vector2{ 500.0f, 300.0f }, 5.0f);
    CHECK_FALSE(f.unlocked());
    CHECK_FALSE(f.failed());

    f.tracker().onPlayerMoved(Vector2{ 500.0f, 300.0f }, 15.0f);
    CHECK(f.unlocked());
}

TEST_CASE("ReachLocation fails once its window has closed", "[secrets][tracker]") {
    Fixture f{ hu::makeReachLocation(hu::makeCircleRegion(500.0f, 300.0f, 50.0f), 10.0f, 30.0f) };

    f.tracker().update(0.016f, 20.0f);
    CHECK_FALSE(f.failed());

    f.tracker().update(0.016f, 30.5f);
    CHECK(f.failed());

    // And a late arrival cannot revive it.
    f.tracker().onPlayerMoved(Vector2{ 500.0f, 300.0f }, 31.0f);
    CHECK_FALSE(f.unlocked());
}

// ---------------------------------------------------------------------------
// DestroyTargets
// ---------------------------------------------------------------------------

TEST_CASE("DestroyTargets counts only the requested archetype", "[secrets][tracker]") {
    Fixture f{ hu::makeDestroyTargets(hu::EnemyArchetype::Turret, 3) };

    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Drifter, "", 1.0f);
    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Mine, "", 2.0f);
    CHECK(f.counter() == 0);

    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Turret, "", 3.0f);
    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Turret, "", 4.0f);
    CHECK(f.counter() == 2);
    CHECK_FALSE(f.unlocked());

    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Turret, "", 5.0f);
    CHECK(f.unlocked());
}

TEST_CASE("DestroyTargets can be restricted to a named wave", "[secrets][tracker]") {
    Fixture f{ hu::makeDestroyTargets(hu::EnemyArchetype::Drifter, 2, 0.0f, "gauntlet") };

    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Drifter, "opening", 1.0f);
    CHECK(f.counter() == 0);

    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Drifter, "gauntlet", 2.0f);
    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Drifter, "gauntlet", 3.0f);
    CHECK(f.unlocked());
}

TEST_CASE("An empty wave name accepts kills from any wave", "[secrets][tracker]") {
    Fixture f{ hu::makeDestroyTargets(hu::EnemyArchetype::Drifter, 2) };

    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Drifter, "opening", 1.0f);
    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Drifter, "gauntlet", 2.0f);
    CHECK(f.unlocked());
}

TEST_CASE("A timed DestroyTargets streak restarts when the limit lapses", "[secrets][tracker]") {
    // Three turrets within five seconds of the first kill.
    Fixture f{ hu::makeDestroyTargets(hu::EnemyArchetype::Turret, 3, 5.0f) };

    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Turret, "", 10.0f);
    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Turret, "", 11.0f);
    CHECK(f.counter() == 2);

    // Too late: this kill starts a fresh streak rather than completing the old
    // one, so the player does not get credit for a slow three.
    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Turret, "", 20.0f);
    CHECK(f.counter() == 1);
    CHECK_FALSE(f.unlocked());

    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Turret, "", 21.0f);
    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Turret, "", 22.0f);
    CHECK(f.unlocked());
}

TEST_CASE("A timed DestroyTargets streak completed inside the limit unlocks", "[secrets][tracker]") {
    Fixture f{ hu::makeDestroyTargets(hu::EnemyArchetype::Turret, 3, 5.0f) };

    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Turret, "", 10.0f);
    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Turret, "", 12.0f);
    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Turret, "", 14.5f);

    CHECK(f.unlocked());
}

TEST_CASE("An idle timed streak expires through update()", "[secrets][tracker]") {
    Fixture f{ hu::makeDestroyTargets(hu::EnemyArchetype::Turret, 3, 5.0f) };

    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Turret, "", 10.0f);
    REQUIRE(f.counter() == 1);

    // Without this the counter would sit at 1 indefinitely and the HUD would
    // report progress the player no longer has.
    f.tracker().update(0.016f, 16.0f);
    CHECK(f.counter() == 0);
    CHECK(f.only().timerStart < 0.0f);
    CHECK_FALSE(f.failed());   // expiring a streak is not a permanent failure
}

// ---------------------------------------------------------------------------
// SurviveWindow / NoFireWindow
// ---------------------------------------------------------------------------

TEST_CASE("SurviveWindow unlocks once the window passes untouched", "[secrets][tracker]") {
    Fixture f{ hu::makeSurviveWindow(10.0f, 20.0f) };

    f.tracker().update(0.016f, 15.0f);
    CHECK_FALSE(f.unlocked());   // still inside the window

    f.tracker().update(0.016f, 20.5f);
    CHECK(f.unlocked());
}

TEST_CASE("SurviveWindow fails on damage inside the window", "[secrets][tracker]") {
    Fixture f{ hu::makeSurviveWindow(10.0f, 20.0f) };

    f.tracker().onPlayerDamaged(5.0f, 15.0f);
    CHECK(f.failed());

    // Surviving the rest of the window cannot undo it.
    f.tracker().update(0.016f, 25.0f);
    CHECK_FALSE(f.unlocked());
}

TEST_CASE("SurviveWindow ignores damage outside the window", "[secrets][tracker]") {
    Fixture f{ hu::makeSurviveWindow(10.0f, 20.0f) };

    f.tracker().onPlayerDamaged(5.0f, 5.0f);    // before
    f.tracker().onPlayerDamaged(5.0f, 25.0f);   // after
    CHECK_FALSE(f.failed());

    f.tracker().update(0.016f, 30.0f);
    CHECK(f.unlocked());
}

TEST_CASE("Zero damage does not break a survive window", "[secrets][tracker]") {
    Fixture f{ hu::makeSurviveWindow(10.0f, 20.0f) };

    // Guarded at the top of onPlayerDamaged; a grazing hit that resolves to no
    // damage should not count as being hit.
    f.tracker().onPlayerDamaged(0.0f, 15.0f);
    f.tracker().onPlayerDamaged(-1.0f, 16.0f);
    CHECK_FALSE(f.failed());

    f.tracker().update(0.016f, 21.0f);
    CHECK(f.unlocked());
}

TEST_CASE("NoFireWindow fails on a shot inside the window", "[secrets][tracker]") {
    Fixture f{ hu::makeNoFireWindow(10.0f, 20.0f) };

    f.tracker().onPlayerFired(9.0f);    // before the window: harmless
    CHECK_FALSE(f.failed());

    f.tracker().onPlayerFired(12.0f);
    CHECK(f.failed());
}

TEST_CASE("NoFireWindow unlocks when the window passes without firing", "[secrets][tracker]") {
    Fixture f{ hu::makeNoFireWindow(10.0f, 20.0f) };

    f.tracker().onPlayerFired(5.0f);
    f.tracker().update(0.016f, 20.5f);

    CHECK(f.unlocked());
}

// ---------------------------------------------------------------------------
// CollectSequence
// ---------------------------------------------------------------------------

TEST_CASE("CollectSequence unlocks on the exact ordered sequence", "[secrets][tracker]") {
    Fixture f{ hu::makeCollectSequence({ hu::PowerupType::WeaponSpread,
                                         hu::PowerupType::WeaponMissile,
                                         hu::PowerupType::WeaponLaser }) };

    f.tracker().onPowerupCollected(hu::PowerupType::WeaponSpread, 1.0f);
    CHECK(f.counter() == 1);
    f.tracker().onPowerupCollected(hu::PowerupType::WeaponMissile, 2.0f);
    CHECK(f.counter() == 2);
    CHECK_FALSE(f.unlocked());

    f.tracker().onPowerupCollected(hu::PowerupType::WeaponLaser, 3.0f);
    CHECK(f.unlocked());
}

TEST_CASE("A wrong pick-up restarts the sequence", "[secrets][tracker]") {
    Fixture f{ hu::makeCollectSequence({ hu::PowerupType::WeaponSpread,
                                         hu::PowerupType::WeaponMissile,
                                         hu::PowerupType::WeaponLaser }) };

    f.tracker().onPowerupCollected(hu::PowerupType::WeaponSpread, 1.0f);
    f.tracker().onPowerupCollected(hu::PowerupType::CellRepair, 2.0f);
    CHECK(f.counter() == 0);
    CHECK_FALSE(f.failed());   // recoverable, unlike a broken survive window

    // Starting over from scratch still works.
    f.tracker().onPowerupCollected(hu::PowerupType::WeaponSpread, 3.0f);
    f.tracker().onPowerupCollected(hu::PowerupType::WeaponMissile, 4.0f);
    f.tracker().onPowerupCollected(hu::PowerupType::WeaponLaser, 5.0f);
    CHECK(f.unlocked());
}

TEST_CASE("A wrong pick-up that is the first step becomes the new first step", "[secrets][tracker]") {
    Fixture f{ hu::makeCollectSequence({ hu::PowerupType::WeaponSpread,
                                         hu::PowerupType::WeaponMissile }) };

    f.tracker().onPowerupCollected(hu::PowerupType::WeaponSpread, 1.0f);
    REQUIRE(f.counter() == 1);

    // Breaking the run with the sequence's own opener should not throw that
    // pick-up away; the player has restarted, not lost their place entirely.
    f.tracker().onPowerupCollected(hu::PowerupType::WeaponSpread, 2.0f);
    CHECK(f.counter() == 1);

    f.tracker().onPowerupCollected(hu::PowerupType::WeaponMissile, 3.0f);
    CHECK(f.unlocked());
}

TEST_CASE("A wrong pick-up before the sequence starts changes nothing", "[secrets][tracker]") {
    Fixture f{ hu::makeCollectSequence({ hu::PowerupType::WeaponSpread,
                                         hu::PowerupType::WeaponMissile }) };

    f.tracker().onPowerupCollected(hu::PowerupType::CellRepair, 1.0f);
    f.tracker().onPowerupCollected(hu::PowerupType::EnergyCharge, 2.0f);
    CHECK(f.counter() == 0);
    CHECK_FALSE(f.failed());
}

// ---------------------------------------------------------------------------
// DefeatBossUnder
// ---------------------------------------------------------------------------

TEST_CASE("DefeatBossUnder unlocks inside the limit and fails outside it", "[secrets][tracker]") {
    SECTION("comfortably inside") {
        Fixture f{ hu::makeDefeatBossUnder(120.0f) };
        f.tracker().onBossDefeated(90.0f);
        CHECK(f.unlocked());
    }

    SECTION("exactly on the limit fails") {
        // The comparison is a strict `<`, so the limit is a time to beat rather
        // than a time to match.
        Fixture f{ hu::makeDefeatBossUnder(120.0f) };
        f.tracker().onBossDefeated(120.0f);
        CHECK_FALSE(f.unlocked());
        CHECK(f.failed());
    }

    SECTION("over the limit fails") {
        Fixture f{ hu::makeDefeatBossUnder(120.0f) };
        f.tracker().onBossDefeated(130.0f);
        CHECK(f.failed());
    }
}

// ---------------------------------------------------------------------------
// FlawlessWave
// ---------------------------------------------------------------------------

TEST_CASE("FlawlessWave unlocks when the wave is cleared undamaged", "[secrets][tracker]") {
    Fixture f{ hu::makeFlawlessWave("gauntlet") };

    f.tracker().onWaveCleared("gauntlet", 30.0f);
    CHECK(f.unlocked());
}

TEST_CASE("FlawlessWave fails when damage lands during the wave", "[secrets][tracker]") {
    Fixture f{ hu::makeFlawlessWave("gauntlet") };

    f.tracker().onPlayerDamaged(10.0f, 15.0f);
    f.tracker().onWaveCleared("gauntlet", 30.0f);

    CHECK(f.failed());
    CHECK_FALSE(f.unlocked());
}

TEST_CASE("FlawlessWave only considers damage taken during its own wave", "[secrets][tracker]") {
    Fixture f{ hu::makeFlawlessWave("second") };

    // Damaged during the first wave, which then clears and moves the wave
    // boundary forward. The second wave starts clean.
    f.tracker().onPlayerDamaged(10.0f, 5.0f);
    f.tracker().onWaveCleared("first", 10.0f);
    f.tracker().onWaveCleared("second", 20.0f);

    CHECK(f.unlocked());
}

TEST_CASE("FlawlessWave ignores waves it is not named for", "[secrets][tracker]") {
    Fixture f{ hu::makeFlawlessWave("gauntlet") };

    f.tracker().onWaveCleared("opening", 10.0f);
    CHECK_FALSE(f.unlocked());
    CHECK_FALSE(f.failed());

    f.tracker().onWaveCleared("gauntlet", 20.0f);
    CHECK(f.unlocked());
}

// ---------------------------------------------------------------------------
// Latching, reporting and lifetime
// ---------------------------------------------------------------------------

TEST_CASE("An unlocked secret is reported once and stays unlocked", "[secrets][tracker]") {
    Fixture f{ hu::makeReachLocation(hu::makeCircleRegion(0.0f, 0.0f, 10.0f)), "s.region" };

    f.tracker().onPlayerMoved(Vector2{ 0.0f, 0.0f }, 1.0f);

    std::vector<const hu::SecretDefinition*> first = f.tracker().takeNewlyUnlocked();
    REQUIRE(first.size() == 1);
    CHECK(first.front()->id == "s.region");

    // Draining is destructive, so the scene cannot show the same toast twice.
    CHECK(f.tracker().takeNewlyUnlocked().empty());

    // Re-entering the region does not re-fire it either.
    f.tracker().onPlayerMoved(Vector2{ 0.0f, 0.0f }, 2.0f);
    CHECK(f.tracker().takeNewlyUnlocked().empty());
    CHECK(f.unlocked());
}

TEST_CASE("The tracker evaluates several secrets independently", "[secrets][tracker]") {
    Fixture f;
    f.add(hu::makeDestroyTargets(hu::EnemyArchetype::Drifter, 2), "s.kills");
    f.add(hu::makeNoFireWindow(10.0f, 20.0f), "s.nofire");
    f.add(hu::makeDefeatBossUnder(60.0f), "s.boss");
    f.start();

    REQUIRE(f.tracker().trackedCount() == 3);

    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Drifter, "", 1.0f);
    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Drifter, "", 2.0f);
    f.tracker().onPlayerFired(15.0f);          // breaks the no-fire secret only
    f.tracker().onBossDefeated(50.0f);

    CHECK(f.tracker().isUnlocked("s.kills"));
    CHECK_FALSE(f.tracker().isUnlocked("s.nofire"));
    CHECK(f.tracker().isUnlocked("s.boss"));
    CHECK(f.tracker().unlockedCount() == 2);

    CHECK(f.tracker().takeNewlyUnlocked().size() == 2);
}

TEST_CASE("isUnlocked reports false for an unknown secret id", "[secrets][tracker]") {
    Fixture f{ hu::makeDefeatBossUnder(60.0f), "s.boss" };

    CHECK_FALSE(f.tracker().isUnlocked("nope.not.here"));
}

TEST_CASE("Starting a level clears the previous run's progress", "[secrets][tracker]") {
    Fixture f{ hu::makeDestroyTargets(hu::EnemyArchetype::Drifter, 3), "s.kills" };

    f.tracker().onEnemyDestroyed(hu::EnemyArchetype::Drifter, "", 1.0f);
    f.tracker().onPlayerDamaged(10.0f, 2.0f);
    REQUIRE(f.counter() == 1);

    f.start();

    CHECK(f.counter() == 0);
    CHECK_FALSE(f.unlocked());
    CHECK(f.tracker().unlockedCount() == 0);
    CHECK(f.tracker().takeNewlyUnlocked().empty());
    CHECK(f.tracker().activeLevelId() == "test_secrets");
}

TEST_CASE("A restarted run does not inherit the previous run's damage", "[secrets][tracker]") {
    // m_lastDamageTime has to be cleared on restart, or the first wave of the
    // new run would be judged against a hit taken in the old one.
    Fixture f{ hu::makeFlawlessWave("opening"), "s.flawless" };

    f.tracker().onPlayerDamaged(10.0f, 5.0f);
    f.start();
    f.tracker().onWaveCleared("opening", 10.0f);

    CHECK(f.unlocked());
}
