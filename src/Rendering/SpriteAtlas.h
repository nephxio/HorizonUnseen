#pragma once

// Maps hu::SpriteId onto normalised UV rectangles inside the packed atlas.
//
// The atlas description is a small hand-written JSON file produced by
// tools/generate_art.py. Rather than pull in a JSON library for one file with a
// fixed shape, this parses it directly; the format is documented in
// src/Core/SpriteId.h.

#include "Core/SpriteId.h"

#include <array>
#include <cstdint>
#include <string>

namespace hu {

// Normalised texture coordinates: (u0, v0) top-left, (u1, v1) bottom-right.
struct UvRect {
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
};

// Pixel rectangle as it appears in atlas.json.
struct AtlasRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

class SpriteAtlas {
public:
    // Parses the given atlas.json. On failure every SpriteId resolves to the
    // full-texture rect, which is exactly what the 1x1 white fallback texture
    // needs. Returns false so the caller can log and pick that fallback.
    bool loadFromFile(const std::string& path);

    // Every SpriteId maps to the whole texture. Used with the generated 1x1
    // white texture when the atlas is unavailable.
    void setIdentityMapping();

    const UvRect& getUv(SpriteId id) const;

    // Source pixel size of a sprite, useful when a caller wants native-size
    // quads. Zero when the atlas failed to load.
    const AtlasRect& getSourceRect(SpriteId id) const;

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    const std::string& getImageName() const { return m_imageName; }

    // Number of SpriteId entries that were found in the atlas.
    std::size_t getResolvedCount() const { return m_resolvedCount; }

private:
    std::array<UvRect, SpriteIdCount> m_uvRects{};
    std::array<AtlasRect, SpriteIdCount> m_sourceRects{};
    std::string m_imageName;
    int m_width = 0;
    int m_height = 0;
    std::size_t m_resolvedCount = 0;
};

} // namespace hu
