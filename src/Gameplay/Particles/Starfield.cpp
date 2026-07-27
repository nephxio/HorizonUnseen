#include "Gameplay/Particles/Starfield.h"

#include "Core/Log.h"
#include "Core/Math.h"

#include <algorithm>

namespace hu {
namespace {

using namespace starfieldconst;

// ---------------------------------------------------------------------------
// Tunable constants
// ---------------------------------------------------------------------------

// Star pixel sizes per depth, far -> near. Nearer stars are larger.
constexpr float FarSizeMin = 1.0f, FarSizeMax = 2.0f;
constexpr float MidSizeMin = 2.0f, MidSizeMax = 3.5f;
constexpr float NearSizeMin = 3.0f, NearSizeMax = 6.0f;

// Brightness (alpha) per depth. Nearer stars are brighter.
constexpr float FarAlphaMin = 0.22f, FarAlphaMax = 0.45f;
constexpr float MidAlphaMin = 0.45f, MidAlphaMax = 0.72f;
constexpr float NearAlphaMin = 0.75f, NearAlphaMax = 1.0f;

// Chance a star uses the larger sprite, per depth.
constexpr float FarLargeChance = 0.0f;
constexpr float MidLargeChance = 0.25f;
constexpr float NearLargeChance = 0.7f;

// Slight colour variation keeps the field from looking like graph paper.
constexpr float StarWarmTint = 0.86f;   // Lower bound for the r/b jitter.
constexpr float NebulaSizeMin = 220.0f;
constexpr float NebulaSizeMax = 520.0f;
constexpr float NebulaAspectMin = 0.55f;
constexpr float NebulaAspectMax = 0.9f;
constexpr float NebulaAlphaMin = 0.05f;
constexpr float NebulaAlphaMax = 0.16f;

// Vertical margin so stars can drift slightly off the top/bottom edges.
constexpr float VerticalMargin = 8.0f;

// Nebulae are tinted from this small palette.
constexpr Color NebulaTints[] = {
    { 0.34f, 0.24f, 0.62f, 1.0f },
    { 0.16f, 0.36f, 0.60f, 1.0f },
    { 0.52f, 0.20f, 0.42f, 1.0f },
    { 0.18f, 0.48f, 0.48f, 1.0f }
};
constexpr std::size_t NebulaTintCount = sizeof(NebulaTints) / sizeof(NebulaTints[0]);

float speedFactorFor(int depthIndex) {
    switch (depthIndex) {
        case 0:  return FarSpeedFactor;
        case 1:  return MidSpeedFactor;
        default: return NearSpeedFactor;
    }
}

} // namespace

Starfield::Starfield(float screenWidth, float screenHeight, std::uint32_t seed)
    : m_width(screenWidth > 1.0f ? screenWidth : 1.0f)
    , m_height(screenHeight > 1.0f ? screenHeight : 1.0f)
    , m_seed(seed)
    , m_rngState(seed) {
    // Reserve up front: nothing below this point ever grows a vector.
    m_stars.reserve(static_cast<std::size_t>(FarStarCount + MidStarCount + NearStarCount));
    m_nebulae.reserve(static_cast<std::size_t>(NebulaCount));
    generate();
    HU_LOG_INFO(LogCategory,
                "Starfield initialised: %zu stars across 3 layers, %zu nebulae, %.0fx%.0f, seed=0x%08X",
                m_stars.size(), m_nebulae.size(), m_width, m_height, m_seed);
}

std::uint32_t Starfield::nextRandom() {
    // xorshift32; zero is not a valid state, so it is nudged if the caller
    // seeds with 0.
    if (m_rngState == 0u) {
        m_rngState = 0x1234567u;
    }
    m_rngState ^= m_rngState << 13;
    m_rngState ^= m_rngState >> 17;
    m_rngState ^= m_rngState << 5;
    return m_rngState;
}

float Starfield::randomFloat() {
    return static_cast<float>(nextRandom() & 0x00FFFFFFu) / static_cast<float>(0x01000000u);
}

float Starfield::randomFloat(float lo, float hi) {
    return lo + randomFloat() * (hi - lo);
}

void Starfield::placeStar(Star& star, Depth depth, bool anywhere) {
    float sizeMin = FarSizeMin, sizeMax = FarSizeMax;
    float alphaMin = FarAlphaMin, alphaMax = FarAlphaMax;
    float largeChance = FarLargeChance;
    if (depth == Depth::Mid) {
        sizeMin = MidSizeMin; sizeMax = MidSizeMax;
        alphaMin = MidAlphaMin; alphaMax = MidAlphaMax;
        largeChance = MidLargeChance;
    } else if (depth == Depth::Near) {
        sizeMin = NearSizeMin; sizeMax = NearSizeMax;
        alphaMin = NearAlphaMin; alphaMax = NearAlphaMax;
        largeChance = NearLargeChance;
    }

    star.depth = depth;
    const float size = randomFloat(sizeMin, sizeMax);
    star.size = Vector2{ size, size };
    star.sprite = randomFloat() < largeChance ? SpriteId::StarLarge : SpriteId::StarSmall;

    // Warm/cool jitter around white.
    const float warm = randomFloat(StarWarmTint, 1.0f);
    const float cool = randomFloat(StarWarmTint, 1.0f);
    star.color = Color{ warm, 1.0f, cool, randomFloat(alphaMin, alphaMax) };

    star.position.y = randomFloat(-VerticalMargin, m_height + VerticalMargin);
    // On generation stars fill the screen; on wrap they re-enter from the right.
    star.position.x = anywhere ? randomFloat(0.0f, m_width)
                               : m_width + randomFloat(0.0f, m_width * 0.25f) + star.size.x;
}

void Starfield::placeNebula(Nebula& nebula, bool anywhere) {
    const float width = randomFloat(NebulaSizeMin, NebulaSizeMax);
    nebula.size = Vector2{ width, width * randomFloat(NebulaAspectMin, NebulaAspectMax) };
    nebula.rotation = randomFloat(0.0f, TwoPi);

    Color tint = NebulaTints[nextRandom() % NebulaTintCount];
    tint.a = randomFloat(NebulaAlphaMin, NebulaAlphaMax);
    nebula.color = tint;

    nebula.position.y = randomFloat(0.0f, m_height);
    nebula.position.x = anywhere ? randomFloat(0.0f, m_width)
                                 : m_width + nebula.size.x * 0.5f + randomFloat(0.0f, m_width * 0.5f);
}

void Starfield::generate() {
    m_rngState = m_seed;
    m_stars.clear();
    m_nebulae.clear();

    const int counts[] = { FarStarCount, MidStarCount, NearStarCount };
    for (int depthIndex = 0; depthIndex < 3; ++depthIndex) {
        for (int i = 0; i < counts[depthIndex]; ++i) {
            Star star;
            placeStar(star, static_cast<Depth>(depthIndex), true);
            m_stars.push_back(star);
        }
    }

    for (int i = 0; i < NebulaCount; ++i) {
        Nebula nebula;
        placeNebula(nebula, true);
        m_nebulae.push_back(nebula);
    }
}

void Starfield::reset(std::uint32_t seed) {
    m_seed = seed;
    generate();
}

void Starfield::resize(float screenWidth, float screenHeight) {
    m_width = screenWidth > 1.0f ? screenWidth : 1.0f;
    m_height = screenHeight > 1.0f ? screenHeight : 1.0f;
    generate();
    HU_LOG_INFO(LogCategory, "Starfield resized to %.0fx%.0f", m_width, m_height);
}

void Starfield::update(float dt, float scrollSpeed) {
    if (dt <= 0.0f) {
        return;
    }

    for (Star& star : m_stars) {
        star.position.x -= scrollSpeed * speedFactorFor(static_cast<int>(star.depth)) * dt;
        // Fully off the left edge: recycle to the right with fresh properties.
        if (star.position.x < -star.size.x) {
            placeStar(star, star.depth, false);
        }
    }

    for (Nebula& nebula : m_nebulae) {
        nebula.position.x -= scrollSpeed * NebulaSpeedFactor * dt;
        if (nebula.position.x < -nebula.size.x * 0.5f) {
            placeNebula(nebula, false);
        }
    }
}

void Starfield::appendDraw(DrawList& out) const {
    // Nebulae first so they sit behind the stars within BackgroundFar (the
    // layer sort is stable, so emission order is preserved).
    for (const Nebula& nebula : m_nebulae) {
        out.add(SpriteId::NebulaPatch, nebula.position, nebula.size,
                DrawLayer::BackgroundFar, nebula.color, nebula.rotation, true);
    }

    for (const Star& star : m_stars) {
        const DrawLayer layer = star.depth == Depth::Near ? DrawLayer::BackgroundNear
                                                          : DrawLayer::BackgroundFar;
        out.add(star.sprite, star.position, star.size, layer, star.color, 0.0f, true);
    }
}

} // namespace hu
