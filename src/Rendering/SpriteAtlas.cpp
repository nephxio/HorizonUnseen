#include "SpriteAtlas.h"

#include "Core/Log.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace hu {
namespace {

constexpr const char* kLogCategory = "SpriteAtlas";

void skipWhitespace(const std::string& text, std::size_t& index) {
    while (index < text.size() &&
           (text[index] == ' ' || text[index] == '\t' || text[index] == '\r' || text[index] == '\n')) {
        ++index;
    }
}

// Reads a double-quoted string. Only the escapes that a generated atlas can
// realistically contain are handled; anything exotic would mean the file was
// not written by tools/generate_art.py.
bool parseString(const std::string& text, std::size_t& index, std::string& out) {
    skipWhitespace(text, index);
    if (index >= text.size() || text[index] != '"') {
        return false;
    }
    ++index;

    out.clear();
    while (index < text.size() && text[index] != '"') {
        if (text[index] == '\\' && index + 1 < text.size()) {
            ++index;
            switch (text[index]) {
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                default:  out.push_back(text[index]); break;
            }
        } else {
            out.push_back(text[index]);
        }
        ++index;
    }

    if (index >= text.size()) {
        return false;
    }
    ++index; // closing quote
    return true;
}

bool expectChar(const std::string& text, std::size_t& index, char expected) {
    skipWhitespace(text, index);
    if (index >= text.size() || text[index] != expected) {
        return false;
    }
    ++index;
    return true;
}

bool parseNumber(const std::string& text, std::size_t& index, double& out) {
    skipWhitespace(text, index);
    const char* begin = text.c_str() + index;
    char* end = nullptr;
    const double value = std::strtod(begin, &end);
    if (end == begin) {
        return false;
    }
    index += static_cast<std::size_t>(end - begin);
    out = value;
    return true;
}

// { "x": 0, "y": 0, "w": 64, "h": 40 }
bool parseRect(const std::string& text, std::size_t& index, AtlasRect& rect) {
    if (!expectChar(text, index, '{')) {
        return false;
    }

    skipWhitespace(text, index);
    if (index < text.size() && text[index] == '}') {
        ++index;
        return true;
    }

    while (true) {
        std::string key;
        if (!parseString(text, index, key)) {
            return false;
        }
        if (!expectChar(text, index, ':')) {
            return false;
        }

        double value = 0.0;
        if (!parseNumber(text, index, value)) {
            return false;
        }

        const int intValue = static_cast<int>(value);
        if (key == "x") {
            rect.x = intValue;
        } else if (key == "y") {
            rect.y = intValue;
        } else if (key == "w") {
            rect.w = intValue;
        } else if (key == "h") {
            rect.h = intValue;
        }

        skipWhitespace(text, index);
        if (index >= text.size()) {
            return false;
        }
        if (text[index] == ',') {
            ++index;
            continue;
        }
        if (text[index] == '}') {
            ++index;
            return true;
        }
        return false;
    }
}

// Locates a top-level `"key"` and leaves index just past the following colon.
bool seekKey(const std::string& text, const std::string& key, std::size_t& index) {
    const std::string quoted = "\"" + key + "\"";
    const std::size_t found = text.find(quoted);
    if (found == std::string::npos) {
        return false;
    }
    index = found + quoted.size();
    return expectChar(text, index, ':');
}

} // namespace

void SpriteAtlas::setIdentityMapping() {
    for (std::size_t i = 0; i < SpriteIdCount; ++i) {
        m_uvRects[i] = UvRect{ 0.0f, 0.0f, 1.0f, 1.0f };
        m_sourceRects[i] = AtlasRect{};
    }
    m_resolvedCount = 0;
}

bool SpriteAtlas::loadFromFile(const std::string& path) {
    setIdentityMapping();

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        HU_LOG_ERROR(kLogCategory, "Could not open atlas description '%s'", path.c_str());
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    const std::string text = buffer.str();

    std::size_t index = 0;
    if (seekKey(text, "image", index)) {
        parseString(text, index, m_imageName);
    }

    double numeric = 0.0;
    if (seekKey(text, "width", index) && parseNumber(text, index, numeric)) {
        m_width = static_cast<int>(numeric);
    }
    if (seekKey(text, "height", index) && parseNumber(text, index, numeric)) {
        m_height = static_cast<int>(numeric);
    }

    if (m_width <= 0 || m_height <= 0) {
        HU_LOG_ERROR(kLogCategory, "Atlas '%s' has invalid dimensions %dx%d", path.c_str(), m_width, m_height);
        setIdentityMapping();
        return false;
    }

    if (!seekKey(text, "sprites", index)) {
        HU_LOG_ERROR(kLogCategory, "Atlas '%s' has no \"sprites\" object", path.c_str());
        setIdentityMapping();
        return false;
    }

    if (!expectChar(text, index, '{')) {
        HU_LOG_ERROR(kLogCategory, "Atlas '%s' has a malformed \"sprites\" object", path.c_str());
        setIdentityMapping();
        return false;
    }

    std::unordered_map<std::string, AtlasRect> entries;
    skipWhitespace(text, index);
    if (index < text.size() && text[index] != '}') {
        while (true) {
            std::string name;
            if (!parseString(text, index, name)) {
                HU_LOG_ERROR(kLogCategory, "Atlas '%s': expected a sprite name", path.c_str());
                break;
            }
            if (!expectChar(text, index, ':')) {
                HU_LOG_ERROR(kLogCategory, "Atlas '%s': missing ':' after '%s'", path.c_str(), name.c_str());
                break;
            }

            AtlasRect rect;
            if (!parseRect(text, index, rect)) {
                HU_LOG_ERROR(kLogCategory, "Atlas '%s': malformed rect for '%s'", path.c_str(), name.c_str());
                break;
            }

            entries.emplace(std::move(name), rect);

            skipWhitespace(text, index);
            if (index < text.size() && text[index] == ',') {
                ++index;
                continue;
            }
            break;
        }
    }

    if (entries.empty()) {
        HU_LOG_ERROR(kLogCategory, "Atlas '%s' contained no sprites", path.c_str());
        setIdentityMapping();
        return false;
    }

    const float inverseWidth = 1.0f / static_cast<float>(m_width);
    const float inverseHeight = 1.0f / static_cast<float>(m_height);

    // Half-texel inset. With linear filtering a UV sitting exactly on a rect
    // boundary blends the neighbouring sprite's edge texel into the result;
    // pulling in by half a texel keeps sampling inside the intended cell.
    const float insetU = inverseWidth * 0.5f;
    const float insetV = inverseHeight * 0.5f;

    // "white" doubles as the fallback for any enum entry the atlas is missing,
    // so resolve it first.
    UvRect fallbackUv{ 0.0f, 0.0f, 1.0f, 1.0f };
    AtlasRect fallbackRect{};
    const auto whiteIt = entries.find(spriteName(SpriteId::White));
    if (whiteIt != entries.end()) {
        fallbackRect = whiteIt->second;
        fallbackUv.u0 = static_cast<float>(fallbackRect.x) * inverseWidth + insetU;
        fallbackUv.v0 = static_cast<float>(fallbackRect.y) * inverseHeight + insetV;
        fallbackUv.u1 = static_cast<float>(fallbackRect.x + fallbackRect.w) * inverseWidth - insetU;
        fallbackUv.v1 = static_cast<float>(fallbackRect.y + fallbackRect.h) * inverseHeight - insetV;
    } else {
        HU_LOG_WARN(kLogCategory, "Atlas '%s' has no 'white' sprite; missing sprites will sample the whole atlas",
                    path.c_str());
    }

    m_resolvedCount = 0;
    for (std::size_t i = 0; i < SpriteIdCount; ++i) {
        const SpriteId id = static_cast<SpriteId>(i);
        const char* name = spriteName(id);

        const auto it = entries.find(name);
        if (it == entries.end()) {
            HU_LOG_WARN(kLogCategory, "SpriteId '%s' is missing from the atlas; falling back to 'white'", name);
            m_uvRects[i] = fallbackUv;
            m_sourceRects[i] = fallbackRect;
            continue;
        }

        const AtlasRect& rect = it->second;
        m_sourceRects[i] = rect;
        m_uvRects[i].u0 = static_cast<float>(rect.x) * inverseWidth + insetU;
        m_uvRects[i].v0 = static_cast<float>(rect.y) * inverseHeight + insetV;
        m_uvRects[i].u1 = static_cast<float>(rect.x + rect.w) * inverseWidth - insetU;
        m_uvRects[i].v1 = static_cast<float>(rect.y + rect.h) * inverseHeight - insetV;
        ++m_resolvedCount;
    }

    return true;
}

const UvRect& SpriteAtlas::getUv(SpriteId id) const {
    const std::size_t index = static_cast<std::size_t>(id);
    if (index >= SpriteIdCount) {
        return m_uvRects[static_cast<std::size_t>(SpriteId::White)];
    }
    return m_uvRects[index];
}

const AtlasRect& SpriteAtlas::getSourceRect(SpriteId id) const {
    const std::size_t index = static_cast<std::size_t>(id);
    if (index >= SpriteIdCount) {
        return m_sourceRects[static_cast<std::size_t>(SpriteId::White)];
    }
    return m_sourceRects[index];
}

} // namespace hu
