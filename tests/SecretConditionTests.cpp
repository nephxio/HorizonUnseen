// Tests for the pure pieces of a secret condition: the region test and the
// optional time window.
//
// These have no state and no dependencies, but both encode conventions that are
// invisible at the call site -- a region is a circle only when radius > 0, and a
// window is "absent" rather than "empty" when windowEnd <= windowStart. Getting
// either backwards makes a secret silently unreachable, which is the failure
// mode this whole area is prone to.

#include "Gameplay/Secrets/SecretDefinition.h"

#include <catch2/catch_test_macros.hpp>

namespace {

constexpr Vector2 point(float x, float y) { return Vector2{ x, y }; }

} // namespace

// ---------------------------------------------------------------------------
// SecretRegion
// ---------------------------------------------------------------------------

TEST_CASE("A circle region contains points within its radius", "[secrets][region]") {
    const hu::SecretRegion region = hu::makeCircleRegion(100.0f, 200.0f, 50.0f);

    CHECK(region.contains(point(100.0f, 200.0f)));   // centre
    CHECK(region.contains(point(140.0f, 200.0f)));   // inside
    CHECK(region.contains(point(100.0f, 160.0f)));

    CHECK_FALSE(region.contains(point(160.0f, 200.0f)));
    CHECK_FALSE(region.contains(point(100.0f, 260.0f)));

    // Diagonal: (30, 40) is exactly 50 from the centre, so it is the boundary
    // case in both directions.
    CHECK(region.contains(point(130.0f, 240.0f)));
    CHECK_FALSE(region.contains(point(131.0f, 241.0f)));
}

TEST_CASE("A circle region includes its boundary", "[secrets][region]") {
    const hu::SecretRegion region = hu::makeCircleRegion(0.0f, 0.0f, 10.0f);

    // The comparison is `<=`, so a player sitting exactly on the edge counts as
    // having reached it. Worth pinning: an exclusive test would make a region
    // placed flush against level geometry unreachable.
    CHECK(region.contains(point(10.0f, 0.0f)));
    CHECK(region.contains(point(0.0f, -10.0f)));
    CHECK_FALSE(region.contains(point(10.001f, 0.0f)));
}

TEST_CASE("A rect region spans from its origin by width and height", "[secrets][region]") {
    // Note the convention: x/y is a corner, not the centre.
    const hu::SecretRegion region = hu::makeRectRegion(10.0f, 20.0f, 100.0f, 40.0f);

    CHECK(region.contains(point(60.0f, 40.0f)));     // middle
    CHECK(region.contains(point(10.0f, 20.0f)));     // origin corner
    CHECK(region.contains(point(110.0f, 60.0f)));    // far corner
    CHECK(region.contains(point(10.0f, 60.0f)));
    CHECK(region.contains(point(110.0f, 20.0f)));

    CHECK_FALSE(region.contains(point(9.99f, 40.0f)));
    CHECK_FALSE(region.contains(point(110.01f, 40.0f)));
    CHECK_FALSE(region.contains(point(60.0f, 19.99f)));
    CHECK_FALSE(region.contains(point(60.0f, 60.01f)));
}

TEST_CASE("A radius makes a region a circle regardless of its width and height", "[secrets][region]") {
    // The two shapes share one struct and `radius > 0` picks between them. A
    // region carrying both would otherwise be ambiguous.
    hu::SecretRegion region = hu::makeRectRegion(0.0f, 0.0f, 1000.0f, 1000.0f);
    region.radius = 10.0f;

    // Well inside the rect, but far outside the circle.
    CHECK_FALSE(region.contains(point(500.0f, 500.0f)));
    CHECK(region.contains(point(5.0f, 5.0f)));
}

TEST_CASE("A zero-size rect region contains only its own corner", "[secrets][region]") {
    const hu::SecretRegion region = hu::makeRectRegion(50.0f, 50.0f, 0.0f, 0.0f);

    CHECK(region.contains(point(50.0f, 50.0f)));
    CHECK_FALSE(region.contains(point(50.1f, 50.0f)));
}

// ---------------------------------------------------------------------------
// Time window
// ---------------------------------------------------------------------------

TEST_CASE("A condition with no window is always in window", "[secrets][window]") {
    const hu::SecretCondition condition =
        hu::makeDestroyTargets(hu::EnemyArchetype::Drifter, 3);

    REQUIRE_FALSE(condition.hasWindow());

    // "No window" must mean unrestricted rather than never-satisfiable, or
    // every untimed secret in the game would be dead on arrival.
    CHECK(condition.inWindow(0.0f));
    CHECK(condition.inWindow(500.0f));
}

TEST_CASE("A window is inclusive at both ends", "[secrets][window]") {
    const hu::SecretCondition condition = hu::makeSurviveWindow(10.0f, 30.0f);

    REQUIRE(condition.hasWindow());

    CHECK(condition.inWindow(10.0f));    // opens
    CHECK(condition.inWindow(20.0f));
    CHECK(condition.inWindow(30.0f));    // closes

    CHECK_FALSE(condition.inWindow(9.99f));
    CHECK_FALSE(condition.inWindow(30.01f));
}

TEST_CASE("A degenerate window counts as no window at all", "[secrets][window]") {
    // hasWindow() is `windowEnd > windowStart`, so these are not empty windows
    // that nothing can satisfy -- they are treated as unrestricted.
    hu::SecretCondition equal = hu::makeSurviveWindow(15.0f, 15.0f);
    CHECK_FALSE(equal.hasWindow());
    CHECK(equal.inWindow(0.0f));

    hu::SecretCondition inverted = hu::makeSurviveWindow(40.0f, 10.0f);
    CHECK_FALSE(inverted.hasWindow());
    CHECK(inverted.inWindow(0.0f));
}
