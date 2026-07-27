#pragma once

// Parallax star background.
//
// Three depth layers scroll left at different rates; the far layer also carries
// a handful of large, slow nebula quads. Everything is generated once from a
// seed and then only moved, so the background is fully deterministic for a
// given seed and never allocates while the game is running.

#include "Core/DrawList.h"
#include "Core/SpriteId.h"
#include "Game/Entity.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace hu {

// ---------------------------------------------------------------------------
// Tunable constants
// ---------------------------------------------------------------------------
namespace starfieldconst {

inline constexpr std::uint32_t DefaultSeed = 0x5EEDBA5Eu;

// Per-layer star counts, far -> near.
inline constexpr int FarStarCount = 150;
inline constexpr int MidStarCount = 90;
inline constexpr int NearStarCount = 45;

// Scroll rate as a fraction of the scrollSpeed passed to update().
inline constexpr float FarSpeedFactor = 0.12f;
inline constexpr float MidSpeedFactor = 0.35f;
inline constexpr float NearSpeedFactor = 0.85f;

// Nebulae sit behind everything and barely move.
inline constexpr int NebulaCount = 5;
inline constexpr float NebulaSpeedFactor = 0.05f;

inline constexpr const char* LogCategory = "Starfield";

} // namespace starfieldconst

// ---------------------------------------------------------------------------
// Starfield
// ---------------------------------------------------------------------------

class Starfield {
public:
    Starfield(float screenWidth, float screenHeight,
              std::uint32_t seed = starfieldconst::DefaultSeed);

    // scrollSpeed is in world pixels/second for the nearest layer; the other
    // layers are scaled fractions of it. Negative values scroll right.
    void update(float dt, float scrollSpeed);

    void appendDraw(DrawList& out) const;

    // Re-lays out the field for a new resolution, preserving the seed.
    void resize(float screenWidth, float screenHeight);

    // Regenerates from scratch. Same seed + same size always gives the same
    // field, which makes the background reproducible in screenshots and tests.
    void reset(std::uint32_t seed);

    std::size_t starCount() const { return m_stars.size(); }
    std::size_t nebulaCount() const { return m_nebulae.size(); }
    std::uint32_t seed() const { return m_seed; }

private:
    // Far and Mid draw at BackgroundFar, Near at BackgroundNear.
    enum class Depth : std::uint8_t { Far = 0, Mid, Near, Count };

    struct Star {
        Vector2 position{ 0.0f, 0.0f };
        Vector2 size{ 2.0f, 2.0f };
        Color color{};
        SpriteId sprite = SpriteId::StarSmall;
        Depth depth = Depth::Far;
    };

    struct Nebula {
        Vector2 position{ 0.0f, 0.0f };
        Vector2 size{ 256.0f, 160.0f };
        Color color{};
        float rotation = 0.0f;
    };

    // Small xorshift so the field does not depend on the shared rand() stream
    // that the particle system churns.
    std::uint32_t nextRandom();
    float randomFloat();
    float randomFloat(float lo, float hi);

    void generate();
    void placeStar(Star& star, Depth depth, bool anywhere);
    void placeNebula(Nebula& nebula, bool anywhere);

    std::vector<Star> m_stars;
    std::vector<Nebula> m_nebulae;
    float m_width = 1280.0f;
    float m_height = 720.0f;
    std::uint32_t m_seed = starfieldconst::DefaultSeed;
    std::uint32_t m_rngState = starfieldconst::DefaultSeed;
};

} // namespace hu
