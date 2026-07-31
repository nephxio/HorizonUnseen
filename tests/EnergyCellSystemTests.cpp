// Tests for the energy cell system.
//
// The cells are simultaneously the health bar and the superweapon fuel tank,
// and the rule that decides which role a given hit plays -- the rolling
// five-second damage rate -- is the single most load-bearing piece of logic in
// the game. It is also invisible: a bug here reads as "the game feels wrong"
// rather than as a crash, which is exactly the kind of thing that survives
// playtesting and unit tests catch immediately.

#include "Config/GameConfig.h"
#include "Gameplay/Power/EnergyCellSystem.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using Catch::Approx;

namespace {

// The window EnergyCellSystem judges the damage rate over. Mirrored from the
// implementation because the tests need to step past it to prove it decays.
constexpr float kWindowSeconds = 5.0f;

// Builds a system against a known configuration.
//
// GameConfig is a mutable singleton and the cell gradient is derived from it,
// so the expected values below would drift the moment someone rebalanced the
// game. Pinning the inputs here means these tests assert on the *rules*, and a
// balance change cannot silently invalidate them.
hu::EnergyCellSystem makeSystem() {
    GameConfig& cfg = GameConfig::getInstance();
    cfg.healthCellMaxHealth = 100.0f;
    cfg.healthCellMaxEnergy = 100.0f;
    for (float& threshold : cfg.healthCellDamageThresholds) {
        threshold = 25.0f;
    }

    // The constructor calls reset(), which reads the config set above.
    return hu::EnergyCellSystem{};
}

// Derived from the pinned config; see the gradient test for where these come
// from. Named so the intent of each magic number below is readable.
constexpr float kCellHealth[5] = { 200.0f, 165.0f, 130.0f, 95.0f, 60.0f };
constexpr float kCellCharge[5] = { 50.0f, 75.0f, 100.0f, 125.0f, 150.0f };
constexpr float kTotalHealth = 650.0f;   // sum of kCellHealth
constexpr float kTotalCharge = 500.0f;   // sum of kCellCharge

// With a 5s window, a lone hit of this size sits exactly on cell 1's 25 dps
// threshold. Anything smaller is absorbed, anything this size or larger breaks.
constexpr float kBoundaryHit = 125.0f;

float totalCharge(const hu::EnergyCellSystem& cells) {
    float total = 0.0f;
    for (std::size_t i = 0; i < hu::EnergyCellSystem::CellCount; ++i) {
        total += cells.cell(i).charge;
    }
    return total;
}

} // namespace

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

TEST_CASE("Cells form a tanky-to-fragile gradient", "[cells]") {
    const hu::EnergyCellSystem cells = makeSystem();

    REQUIRE(hu::EnergyCellSystem::CellCount == 5);

    for (std::size_t i = 0; i < hu::EnergyCellSystem::CellCount; ++i) {
        INFO("cell " << i + 1);
        CHECK(cells.cell(i).maxHealth == Approx(kCellHealth[i]));
        CHECK(cells.cell(i).maxCharge == Approx(kCellCharge[i]));
        CHECK(cells.cell(i).health == Approx(kCellHealth[i]));
        CHECK(cells.cell(i).charge == Approx(0.0f));
        CHECK_FALSE(cells.cell(i).broken);
    }

    // The design premise: cell 1 is the armoured battery, cell 5 the fragile
    // capacitor holding the most energy. Assert the direction rather than only
    // the values, so a rebalance that inverted the gradient would fail here
    // even if it kept the same numbers in play.
    for (std::size_t i = 1; i < hu::EnergyCellSystem::CellCount; ++i) {
        CHECK(cells.cell(i).maxHealth < cells.cell(i - 1).maxHealth);
        CHECK(cells.cell(i).maxCharge > cells.cell(i - 1).maxCharge);
        CHECK(cells.cell(i).damageRateThreshold > cells.cell(i - 1).damageRateThreshold);
    }

    CHECK(cells.totalMaxHealth() == Approx(kTotalHealth));
    CHECK(cells.totalHealth() == Approx(kTotalHealth));
    CHECK(cells.brokenCellCount() == 0);
    CHECK(cells.isAlive());
    CHECK(cells.chargedCellCount() == 0);
}

// ---------------------------------------------------------------------------
// The absorb-or-break decision
// ---------------------------------------------------------------------------

TEST_CASE("A hit below the damage-rate threshold charges cells instead of hurting them", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();

    cells.applyDamage(40.0f, 0.0f);   // 40 over a 5s window = 8 dps, well under 25

    CHECK(cells.windowedDamageRate() == Approx(8.0f));
    CHECK(cells.totalHealth() == Approx(kTotalHealth));   // nothing lost
    CHECK(cells.cell(0).charge == Approx(40.0f));
    CHECK(cells.cell(1).charge == Approx(0.0f));
}

TEST_CASE("Charge fills from cell 1 upward, overflowing as each fills", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();

    // Two absorbed hits: 80 total is 16 dps, still under the threshold, but
    // more than cell 1's 50-point capacity.
    cells.applyDamage(40.0f, 0.0f);
    cells.applyDamage(40.0f, 1.0f);

    CHECK(cells.totalHealth() == Approx(kTotalHealth));
    CHECK(cells.cell(0).charge == Approx(kCellCharge[0]));   // full
    CHECK(cells.cell(0).isCharged());
    CHECK(cells.cell(1).charge == Approx(30.0f));            // overflow landed here
    CHECK(cells.cell(2).charge == Approx(0.0f));
    CHECK(cells.chargedCellCount() == 1);
}

TEST_CASE("A hit above the threshold strips health from the top cell down", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();

    // 130 over the window is 26 dps, just past cell 1's 25 dps threshold.
    cells.applyDamage(130.0f, 0.0f);

    // Cell 5 (60 hp) absorbs what it can and breaks; the remaining 70 carries
    // down into cell 4.
    CHECK(cells.cell(4).broken);
    CHECK(cells.cell(4).health == Approx(0.0f));
    CHECK_FALSE(cells.cell(3).broken);
    CHECK(cells.cell(3).health == Approx(kCellHealth[3] - 70.0f));

    // Damage never skips down into the lower cells while higher ones survive.
    CHECK(cells.cell(0).health == Approx(kCellHealth[0]));
    CHECK(cells.cell(1).health == Approx(kCellHealth[1]));
    CHECK(cells.cell(2).health == Approx(kCellHealth[2]));

    CHECK(cells.brokenCellCount() == 1);
    CHECK(cells.isAlive());
    CHECK(totalCharge(cells) == Approx(0.0f));   // a breaking hit charges nothing
}

TEST_CASE("A hit counts toward the window it is judged against", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();

    // The window is empty, so the rate before this hit is zero. If the incoming
    // hit were judged against only the *prior* window it would be absorbed and
    // a single arbitrarily large hit could never hurt the player.
    REQUIRE(cells.windowedDamageRate() == Approx(0.0f));

    cells.applyDamage(300.0f, 0.0f);

    CHECK(cells.totalHealth() < kTotalHealth);
    CHECK(cells.brokenCellCount() > 0);
}

TEST_CASE("The absorb/break boundary is exact", "[cells]") {
    SECTION("just under the threshold absorbs") {
        hu::EnergyCellSystem cells = makeSystem();
        cells.applyDamage(kBoundaryHit - 1.0f, 0.0f);

        CHECK(cells.totalHealth() == Approx(kTotalHealth));
        CHECK(totalCharge(cells) == Approx(kBoundaryHit - 1.0f));
    }

    SECTION("landing exactly on the threshold breaks") {
        hu::EnergyCellSystem cells = makeSystem();
        cells.applyDamage(kBoundaryHit, 0.0f);

        // The comparison is `rate < threshold`, so equality is destructive.
        CHECK(cells.totalHealth() < kTotalHealth);
        CHECK(totalCharge(cells) == Approx(0.0f));
    }
}

TEST_CASE("Sustained fire eventually flips from charging to breaking", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();

    // Four hits of 30, one per second. Each is individually harmless, but they
    // accumulate in the window: 30, 60, 90, 120 -> 6, 12, 18, 24 dps.
    for (int i = 0; i < 4; ++i) {
        cells.applyDamage(30.0f, static_cast<float>(i));
    }

    REQUIRE(cells.totalHealth() == Approx(kTotalHealth));
    REQUIRE(totalCharge(cells) == Approx(120.0f));

    // The fifth pushes the window to 150, or 30 dps -- over the line. The same
    // sized hit that was charging the ship a second ago now breaks it.
    cells.applyDamage(30.0f, 4.0f);

    CHECK(cells.windowedDamageRate() == Approx(30.0f));
    CHECK(cells.cell(4).health == Approx(kCellHealth[4] - 30.0f));
    CHECK(cells.totalHealth() == Approx(kTotalHealth - 30.0f));
}

// ---------------------------------------------------------------------------
// The sliding window
//
// This is the property that motivated replacing the old HealthSystem, which
// measured an instantaneous per-frame rate and so behaved differently at
// different frame rates.
// ---------------------------------------------------------------------------

TEST_CASE("The damage window decays, so spread-out fire stays survivable", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();

    cells.applyDamage(100.0f, 0.0f);
    REQUIRE(cells.windowedDamageRate() == Approx(20.0f));

    // Nothing has hit the player since, so the rate must fall on its own.
    cells.update(1.0f / 60.0f, kWindowSeconds + 1.0f);
    CHECK(cells.windowedDamageRate() == Approx(0.0f));

    // And with the window clear, an identical hit is judged on its own again
    // rather than stacking with ancient history.
    cells.applyDamage(100.0f, kWindowSeconds + 1.0f);
    CHECK(cells.windowedDamageRate() == Approx(20.0f));
    CHECK(cells.totalHealth() == Approx(kTotalHealth));
}

TEST_CASE("Damage leaves the window only once it is older than the window length", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();

    cells.applyDamage(100.0f, 0.0f);

    // Still inside the window: the hit must keep counting.
    cells.update(1.0f / 60.0f, kWindowSeconds - 0.5f);
    CHECK(cells.windowedDamageRate() == Approx(20.0f));

    // Now past it.
    cells.update(1.0f / 60.0f, kWindowSeconds + 0.5f);
    CHECK(cells.windowedDamageRate() == Approx(0.0f));
}

// ---------------------------------------------------------------------------
// Charge, spending and the threshold fallback
// ---------------------------------------------------------------------------

TEST_CASE("A fully charged ship shrugs off a hit that would break an empty one", "[cells]") {
    // Once every cell is full there is no cell waiting to receive charge, so
    // the threshold falls back to the highest unbroken cell -- the most
    // tolerant one. A charged ship is genuinely tougher, not just better armed.
    const float hit = 150.0f;   // 30 dps: over cell 1's 25, under cell 5's 40

    SECTION("empty cells: the hit breaks through") {
        hu::EnergyCellSystem cells = makeSystem();
        cells.applyDamage(hit, 0.0f);

        CHECK(cells.totalHealth() < kTotalHealth);
        CHECK(cells.cell(4).broken);
    }

    SECTION("full cells: the same hit is absorbed harmlessly") {
        hu::EnergyCellSystem cells = makeSystem();
        cells.addCharge(kTotalCharge);
        REQUIRE(cells.chargedCellCount() == 5);

        cells.applyDamage(hit, 0.0f);

        CHECK(cells.totalHealth() == Approx(kTotalHealth));
        CHECK(cells.brokenCellCount() == 0);
    }
}

TEST_CASE("A broken cell cannot hold charge", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();
    cells.addCharge(kTotalCharge);
    REQUIRE(cells.chargedCellCount() == 5);

    // Past the fallback threshold of 40 dps, so it lands on health even with
    // every cell charged.
    cells.applyDamage(210.0f, 0.0f);

    REQUIRE(cells.cell(4).broken);
    CHECK(cells.cell(4).charge == Approx(0.0f));
    REQUIRE(cells.cell(3).broken);
    CHECK(cells.cell(3).charge == Approx(0.0f));

    // Cells that survived keep theirs.
    CHECK(cells.cell(2).charge == Approx(kCellCharge[2]));
    CHECK(cells.chargedCellCount() == 3);
}

TEST_CASE("addCharge fills upward and clamps at capacity", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();

    cells.addCharge(60.0f);
    CHECK(cells.cell(0).charge == Approx(kCellCharge[0]));
    CHECK(cells.cell(1).charge == Approx(10.0f));

    // Far more than the stack can hold; the excess evaporates rather than
    // pushing any cell past its maximum.
    cells.addCharge(10'000.0f);
    for (std::size_t i = 0; i < hu::EnergyCellSystem::CellCount; ++i) {
        INFO("cell " << i + 1);
        CHECK(cells.cell(i).charge == Approx(kCellCharge[i]));
    }
    CHECK(cells.chargedCellCount() == 5);
}

TEST_CASE("consumeCharge spends the highest charged cells first", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();
    cells.addCharge(kTotalCharge);

    REQUIRE(cells.consumeCharge(2));

    // Draining from the top leaves the small, quick-to-refill cells holding the
    // remaining energy.
    CHECK(cells.cell(4).charge == Approx(0.0f));
    CHECK(cells.cell(3).charge == Approx(0.0f));
    CHECK(cells.cell(0).charge == Approx(kCellCharge[0]));
    CHECK(cells.cell(1).charge == Approx(kCellCharge[1]));
    CHECK(cells.cell(2).charge == Approx(kCellCharge[2]));
    CHECK(cells.chargedCellCount() == 3);
}

TEST_CASE("consumeCharge refuses when too few cells are charged", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();
    cells.addCharge(kCellCharge[0]);
    REQUIRE(cells.chargedCellCount() == 1);

    SECTION("asking for more than is charged spends nothing") {
        CHECK_FALSE(cells.consumeCharge(2));
        CHECK(cells.chargedCellCount() == 1);
        CHECK(cells.cell(0).charge == Approx(kCellCharge[0]));
    }

    SECTION("non-positive counts are rejected") {
        CHECK_FALSE(cells.consumeCharge(0));
        CHECK_FALSE(cells.consumeCharge(-1));
        CHECK(cells.chargedCellCount() == 1);
    }
}

TEST_CASE("Firing discards partial charge stranded above the consumed cells", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();

    // Cells 1-3 full, with 20 points of progress sitting in cell 4.
    cells.addCharge(kCellCharge[0] + kCellCharge[1] + kCellCharge[2] + 20.0f);
    REQUIRE(cells.chargedCellCount() == 3);
    REQUIRE(cells.cell(3).charge == Approx(20.0f));

    REQUIRE(cells.consumeCharge(3));

    // Charge always refills from the lowest unfilled cell, so a partial cell
    // left floating above empties could never be spent -- it would sit there
    // forever. Firing takes it along with the cells it consumed.
    CHECK(cells.cell(3).charge == Approx(0.0f));
    CHECK(totalCharge(cells) == Approx(0.0f));
    CHECK(cells.chargedCellCount() == 0);
}

// ---------------------------------------------------------------------------
// Repair, death and input guards
// ---------------------------------------------------------------------------

TEST_CASE("Repair restores the lowest broken cell", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();
    cells.applyDamage(210.0f, 0.0f);   // breaks cells 5 and 4
    REQUIRE(cells.brokenCellCount() == 2);

    cells.repairLowestBrokenCell();

    // Cell 4 is the lower of the two, so it comes back first -- and comes back
    // empty, not carrying charge it never earned.
    CHECK_FALSE(cells.cell(3).broken);
    CHECK(cells.cell(3).health == Approx(kCellHealth[3]));
    CHECK(cells.cell(3).charge == Approx(0.0f));

    CHECK(cells.cell(4).broken);
    CHECK(cells.brokenCellCount() == 1);
}

TEST_CASE("Repair is a no-op when nothing is broken", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();
    cells.addCharge(60.0f);

    cells.repairLowestBrokenCell();

    CHECK(cells.totalHealth() == Approx(kTotalHealth));
    CHECK(cells.cell(0).charge == Approx(kCellCharge[0]));
    CHECK(cells.cell(1).charge == Approx(10.0f));
}

TEST_CASE("The ship dies once every cell is broken", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();

    cells.applyDamage(kTotalHealth + 50.0f, 0.0f);

    CHECK_FALSE(cells.isAlive());
    CHECK(cells.brokenCellCount() == hu::EnergyCellSystem::CellCount);
    CHECK(cells.totalHealth() == Approx(0.0f));
}

TEST_CASE("A dead ship ignores further damage", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();
    cells.applyDamage(kTotalHealth + 50.0f, 0.0f);
    REQUIRE_FALSE(cells.isAlive());

    const float rateAtDeath = cells.windowedDamageRate();
    cells.applyDamage(500.0f, 1.0f);

    // Nothing should keep accumulating against a corpse; the death readout the
    // HUD shows would otherwise keep climbing after the run ended.
    CHECK(cells.windowedDamageRate() == Approx(rateAtDeath));
    CHECK(cells.brokenCellCount() == hu::EnergyCellSystem::CellCount);
}

TEST_CASE("Degenerate damage values are ignored", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();

    cells.applyDamage(0.0f, 0.0f);
    cells.applyDamage(-10.0f, 0.0f);
    cells.applyDamage(1e-6f, 0.0f);

    // None of these should register as a hit -- an unguarded negative would
    // *heal* the window and skew the absorb/break decision.
    CHECK(cells.windowedDamageRate() == Approx(0.0f));
    CHECK(cells.totalHealth() == Approx(kTotalHealth));
    CHECK(totalCharge(cells) == Approx(0.0f));
}

TEST_CASE("cell() clamps out-of-range indices", "[cells]") {
    const hu::EnergyCellSystem cells = makeSystem();

    // Callers index this from HUD code driven by loop counters; clamping keeps
    // an off-by-one from reading past the array.
    CHECK(cells.cell(99).maxHealth == Approx(cells.cell(hu::EnergyCellSystem::CellCount - 1).maxHealth));
}

TEST_CASE("reset() returns the stack to its starting state", "[cells]") {
    hu::EnergyCellSystem cells = makeSystem();
    cells.applyDamage(210.0f, 0.0f);
    cells.addCharge(80.0f);
    REQUIRE(cells.brokenCellCount() > 0);

    cells.reset();

    CHECK(cells.totalHealth() == Approx(kTotalHealth));
    CHECK(cells.brokenCellCount() == 0);
    CHECK(cells.chargedCellCount() == 0);
    CHECK(totalCharge(cells) == Approx(0.0f));
    CHECK(cells.windowedDamageRate() == Approx(0.0f));
    CHECK(cells.isAlive());
}
