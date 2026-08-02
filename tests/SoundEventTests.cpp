// Tests for the gameplay side of audio.
//
// Gameplay emits SoundEvents without knowing whether anything is listening, so
// none of this needs a sound device -- which is the point. If these rules break,
// the game goes quiet in ways that no crash and no failing episode would reveal.

#include "Config/GameConfig.h"
#include "Core/SaveGame.h"
#include "Core/SoundId.h"
#include "Gameplay/GameWorld.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <vector>

using Catch::Approx;

namespace {

// Starts a world on the shipped level so the sound queue can be driven.
std::unique_ptr<hu::GameWorld> makeWorld() {
    auto world = std::make_unique<hu::GameWorld>();
    REQUIRE(world->startLevel("test_level", hu::DifficultyMode::Normal));
    // startLevel emits its own effects; drop them so each test starts clean.
    world->takeSoundEvents();
    return world;
}

bool contains(const std::vector<hu::SoundEvent>& events, hu::SoundId id) {
    return std::any_of(events.begin(), events.end(),
                       [id](const hu::SoundEvent& e) { return e.id == id; });
}

hu::EffectRequest effectAt(hu::EffectKind kind, float x = 640.0f) {
    hu::EffectRequest fx;
    fx.kind = kind;
    fx.position = Vector2{ x, 360.0f };
    return fx;
}

} // namespace

// ---------------------------------------------------------------------------
// The queue
// ---------------------------------------------------------------------------

TEST_CASE("Sound events drain like notices", "[audio][events]") {
    auto world = makeWorld();

    world->playSound(hu::SoundId::SecretFound);
    world->playSound(hu::SoundId::UiSelect);

    const std::vector<hu::SoundEvent> first = world->takeSoundEvents();
    CHECK(first.size() == 2);
    CHECK(contains(first, hu::SoundId::SecretFound));

    // Draining is destructive; a second consumer must not replay them.
    CHECK(world->takeSoundEvents().empty());
}

TEST_CASE("The queue is capped so an undrained world cannot grow forever", "[audio][events]") {
    // The headless simulation in src/Sim steps the world millions of times and
    // never calls takeSoundEvents(). Without a ceiling that is an unbounded
    // leak in the RL environment.
    auto world = makeWorld();

    for (int i = 0; i < 5000; ++i) {
        world->playSound(hu::SoundId::Impact);
    }

    const std::vector<hu::SoundEvent> events = world->takeSoundEvents();
    CHECK(events.size() <= 256);
    CHECK_FALSE(events.empty());
}

TEST_CASE("Starting a level clears queued sound", "[audio][events]") {
    auto world = makeWorld();
    world->playSound(hu::SoundId::Explosion);

    REQUIRE(world->startLevel("test_level", hu::DifficultyMode::Normal));

    // Whatever is queued now belongs to the new run, not the old one.
    const std::vector<hu::SoundEvent> events = world->takeSoundEvents();
    CHECK_FALSE(contains(events, hu::SoundId::Explosion));
}

// ---------------------------------------------------------------------------
// Panning
// ---------------------------------------------------------------------------

TEST_CASE("Sounds are panned by where they happened", "[audio][events]") {
    auto world = makeWorld();
    const float width = GameConfig::getInstance().screenWidth;

    world->playSoundAt(hu::SoundId::Impact, Vector2{ 0.0f, 360.0f });
    world->playSoundAt(hu::SoundId::Impact, Vector2{ width * 0.5f, 360.0f });
    world->playSoundAt(hu::SoundId::Impact, Vector2{ width, 360.0f });

    const std::vector<hu::SoundEvent> events = world->takeSoundEvents();
    REQUIRE(events.size() == 3);
    CHECK(events[0].pan == Approx(-1.0f));
    CHECK(events[1].pan == Approx(0.0f));
    CHECK(events[2].pan == Approx(1.0f));
}

TEST_CASE("Off-screen sounds stay at the edge rather than wrapping", "[audio][events]") {
    // An enemy dying past the right edge must not jump to the left speaker.
    auto world = makeWorld();
    const float width = GameConfig::getInstance().screenWidth;

    world->playSoundAt(hu::SoundId::Explosion, Vector2{ -5000.0f, 360.0f });
    world->playSoundAt(hu::SoundId::Explosion, Vector2{ width + 5000.0f, 360.0f });

    const std::vector<hu::SoundEvent> events = world->takeSoundEvents();
    REQUIRE(events.size() == 2);
    CHECK(events[0].pan == Approx(-1.0f));
    CHECK(events[1].pan == Approx(1.0f));
}

TEST_CASE("Non-positional sounds are centred", "[audio][events]") {
    auto world = makeWorld();
    world->playSound(hu::SoundId::SecretFound);

    const std::vector<hu::SoundEvent> events = world->takeSoundEvents();
    REQUIRE(events.size() == 1);
    CHECK(events[0].pan == Approx(0.0f));
}

// ---------------------------------------------------------------------------
// Effects carry their own sound
// ---------------------------------------------------------------------------

TEST_CASE("Spawning an effect also queues its sound", "[audio][events]") {
    // This is what stops ~50 call sites needing a second call each. If it
    // regresses, the game loses almost all of its audio at once.
    auto world = makeWorld();

    world->spawnEffect(effectAt(hu::EffectKind::Explosion));
    CHECK(contains(world->takeSoundEvents(), hu::SoundId::Explosion));

    world->spawnEffect(effectAt(hu::EffectKind::CellBreak));
    CHECK(contains(world->takeSoundEvents(), hu::SoundId::CellBreak));

    world->spawnEffect(effectAt(hu::EffectKind::PowerupPickup));
    CHECK(contains(world->takeSoundEvents(), hu::SoundId::PowerupPickup));
}

TEST_CASE("Every audible effect kind maps to a sound", "[audio][events]") {
    // Thruster is deliberately silent: it is emitted continuously while the
    // ship moves, so a one-shot per emission would be a rattle, not an engine.
    const hu::EffectKind audible[] = {
        hu::EffectKind::MuzzleFlash,   hu::EffectKind::Impact,
        hu::EffectKind::Explosion,     hu::EffectKind::BigExplosion,
        hu::EffectKind::PowerupPickup, hu::EffectKind::CellBreak,
        hu::EffectKind::CellCharge,    hu::EffectKind::SuperweaponCharge,
        hu::EffectKind::ScreenClear,   hu::EffectKind::Debris,
    };

    for (hu::EffectKind kind : audible) {
        auto world = makeWorld();
        world->spawnEffect(effectAt(kind));
        INFO("effect kind index " << static_cast<int>(kind));
        CHECK_FALSE(world->takeSoundEvents().empty());
    }
}

TEST_CASE("The thruster effect stays silent", "[audio][events]") {
    auto world = makeWorld();
    world->spawnEffect(effectAt(hu::EffectKind::Thruster));
    CHECK(world->takeSoundEvents().empty());
}

TEST_CASE("A silent effect request suppresses its sound", "[audio][events]") {
    // Used where a call site borrows an effect's visuals but wants its own
    // sound -- grazing reuses the cell-charge burst.
    auto world = makeWorld();

    hu::EffectRequest fx = effectAt(hu::EffectKind::Explosion);
    fx.silent = true;
    world->spawnEffect(fx);

    CHECK(world->takeSoundEvents().empty());
}

// ---------------------------------------------------------------------------
// Volume persistence
// ---------------------------------------------------------------------------

TEST_CASE("Volumes clamp to a sane range", "[audio][settings]") {
    hu::SaveGame save;

    save.setMasterVolume(2.5f);
    CHECK(save.masterVolume() == Approx(1.0f));

    save.setSfxVolume(-3.0f);
    CHECK(save.sfxVolume() == Approx(0.0f));

    save.setMusicVolume(0.42f);
    CHECK(save.musicVolume() == Approx(0.42f));
}

TEST_CASE("A NaN volume from a corrupt file becomes silence, not chaos", "[audio][settings]") {
    hu::SaveGame save;
    save.setMasterVolume(std::numeric_limits<float>::quiet_NaN());
    // NaN compares false against everything, so a naive clamp would pass it
    // straight through to OpenAL.
    CHECK(save.masterVolume() == Approx(0.0f));
}

TEST_CASE("Volumes survive a save and load round trip", "[audio][settings]") {
    const std::string path = "saves/test_audio_roundtrip.dat";

    {
        hu::SaveGame save;
        save.setMasterVolume(0.31f);
        save.setSfxVolume(0.62f);
        save.setMusicVolume(0.93f);
        REQUIRE(save.save(path));
    }

    hu::SaveGame reloaded;
    REQUIRE(reloaded.load(path));
    CHECK(reloaded.masterVolume() == Approx(0.31f));
    CHECK(reloaded.sfxVolume() == Approx(0.62f));
    CHECK(reloaded.musicVolume() == Approx(0.93f));

    std::remove(path.c_str());
}

TEST_CASE("Resetting progress leaves the mixer alone", "[audio][settings]") {
    // Volumes are a setting rather than progress; wiping a save should not also
    // reset the player's mix.
    hu::SaveGame save;
    save.setMasterVolume(0.25f);

    save.resetProgress();

    CHECK(save.masterVolume() == Approx(0.25f));
}
