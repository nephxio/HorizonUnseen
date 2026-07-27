#include "Core/SaveGame.h"

#include "Core/Log.h"
#include "Gameplay/Secrets/SecretRegistry.h"

#include <filesystem>
#include <fstream>

namespace hu {

namespace {

constexpr const char* kLogCategory = "Save";

template <typename T>
void writePod(std::ostream& stream, const T& value) {
    stream.write(reinterpret_cast<const char*>(&value), sizeof(T));
}

template <typename T>
bool readPod(std::istream& stream, T& value) {
    stream.read(reinterpret_cast<char*>(&value), sizeof(T));
    return static_cast<bool>(stream);
}

void writeString(std::ostream& stream, const std::string& text) {
    const std::uint32_t length = static_cast<std::uint32_t>(text.size());
    writePod(stream, length);
    if (length > 0) {
        stream.write(text.data(), static_cast<std::streamsize>(length));
    }
}

bool readString(std::istream& stream, std::string& out, std::uint32_t maxLength) {
    std::uint32_t length = 0;
    if (!readPod(stream, length)) {
        return false;
    }
    if (length > maxLength) {
        return false;
    }
    out.assign(length, '\0');
    if (length > 0) {
        stream.read(&out[0], static_cast<std::streamsize>(length));
        if (!stream) {
            return false;
        }
    }
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Instance / lifecycle
// ---------------------------------------------------------------------------

SaveGame& SaveGame::instance() {
    static SaveGame s_instance;
    return s_instance;
}

void SaveGame::resetProgress() {
    m_unlockedSecrets.clear();
    m_levels.clear();
    m_lastError.clear();
    recomputeBulletHell();
    HU_LOG_INFO(kLogCategory, "Progress reset to a fresh profile");
}

// ---------------------------------------------------------------------------
// Derived state
// ---------------------------------------------------------------------------

void SaveGame::recomputeBulletHell() {
    // Derived purely from the registries: every secret of every registered
    // level must be present in the unlocked set. No stored flag exists that
    // could drift out of step with the content.
    const std::size_t total = SecretRegistry::totalCount();
    const std::size_t found = secretsFoundTotal();
    m_bulletHellUnlocked = (total > 0) && (found >= total);
}

std::size_t SaveGame::secretsTotal() const {
    return SecretRegistry::totalCount();
}

std::size_t SaveGame::secretsFoundTotal() const {
    std::size_t found = 0;
    for (const LevelDefinition& level : LevelRegistry::all()) {
        found += secretsFoundInLevel(level.id);
    }
    return found;
}

std::size_t SaveGame::secretsTotalInLevel(const std::string& levelId) const {
    return SecretRegistry::countForLevel(levelId);
}

std::size_t SaveGame::secretsFoundInLevel(const std::string& levelId) const {
    std::size_t found = 0;
    for (const SecretDefinition* definition : SecretRegistry::forLevel(levelId)) {
        if (m_unlockedSecrets.count(definition->id) > 0) {
            ++found;
        }
    }
    return found;
}

// ---------------------------------------------------------------------------
// Secrets
// ---------------------------------------------------------------------------

bool SaveGame::unlockSecret(const std::string& secretId) {
    if (secretId.empty()) {
        return false;
    }
    const bool inserted = m_unlockedSecrets.insert(secretId).second;
    if (!inserted) {
        return false;
    }

    if (SecretRegistry::find(secretId) == nullptr) {
        HU_LOG_WARN(kLogCategory,
                    "Unlocked secret '%s' is not in the registry; it will be kept in the "
                    "save but ignored for progression",
                    secretId.c_str());
    }

    const bool wasUnlocked = m_bulletHellUnlocked;
    recomputeBulletHell();
    HU_LOG_INFO(kLogCategory, "Secret recorded: %s (%zu/%zu found)",
                secretId.c_str(), secretsFoundTotal(), secretsTotal());
    if (!wasUnlocked && m_bulletHellUnlocked) {
        HU_LOG_INFO(kLogCategory, "All secrets found - BULLET HELL MODE UNLOCKED");
    }
    return true;
}

bool SaveGame::isSecretUnlocked(const std::string& secretId) const {
    return m_unlockedSecrets.count(secretId) > 0;
}

// ---------------------------------------------------------------------------
// Levels
// ---------------------------------------------------------------------------

void SaveGame::markLevelPlayed(const std::string& levelId) {
    if (levelId.empty()) {
        return;
    }
    m_levels[levelId].played = true;
}

void SaveGame::markLevelCompleted(const std::string& levelId, float timeSeconds, std::int32_t score) {
    if (levelId.empty()) {
        return;
    }
    LevelProgress& progress = m_levels[levelId];
    progress.played = true;
    progress.completed = true;
    if (progress.bestTimeSeconds <= 0.0f || (timeSeconds > 0.0f && timeSeconds < progress.bestTimeSeconds)) {
        progress.bestTimeSeconds = timeSeconds;
    }
    if (score > progress.bestScore) {
        progress.bestScore = score;
    }
    HU_LOG_INFO(kLogCategory, "Level '%s' completed: time %.2fs (best %.2fs), score %d (best %d)",
                levelId.c_str(), timeSeconds, progress.bestTimeSeconds, score, progress.bestScore);
}

const LevelProgress* SaveGame::levelProgress(const std::string& levelId) const {
    const auto it = m_levels.find(levelId);
    return (it == m_levels.end()) ? nullptr : &it->second;
}

// ---------------------------------------------------------------------------
// Persistence
//
// File layout (little endian, matching ConfigManager's raw-POD style):
//   u32 magic 'HUSG'
//   u32 version
//   u32 secretCount
//     repeated: u32 length + bytes   (secret id)
//   u32 levelCount
//     repeated: u32 length + bytes   (level id)
//               u8  played
//               u8  completed
//               f32 bestTimeSeconds
//               i32 bestScore
// ---------------------------------------------------------------------------

bool SaveGame::load(const std::string& path) {
    m_lastError.clear();

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        // Entirely normal on a first run.
        resetProgress();
        HU_LOG_INFO(kLogCategory, "No save file at '%s'; starting a fresh profile", path.c_str());
        m_lastError = "Save file not found: " + path;
        return false;
    }

    const auto bail = [&](const std::string& reason) {
        m_lastError = reason;
        HU_LOG_WARN(kLogCategory, "Save file '%s' rejected (%s); falling back to defaults",
                    path.c_str(), reason.c_str());
        resetProgress();
        m_lastError = reason;
        return false;
    };

    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    if (!readPod(file, magic) || !readPod(file, version)) {
        return bail("truncated header");
    }
    if (magic != kMagic) {
        return bail("magic number mismatch");
    }
    if (version > kVersion) {
        return bail("save was written by a newer build (version " + std::to_string(version) + ")");
    }
    if (version < kVersion) {
        // Older versions are readable today because version 1 is the first
        // layout; keep the branch so future migrations have an obvious home.
        HU_LOG_WARN(kLogCategory, "Save file '%s' is version %u (current %u); upgrading on next save",
                    path.c_str(), version, kVersion);
    }

    std::set<std::string> secrets;
    std::map<std::string, LevelProgress> levels;

    std::uint32_t secretCount = 0;
    if (!readPod(file, secretCount)) {
        return bail("truncated secret count");
    }
    if (secretCount > kMaxRecords) {
        return bail("implausible secret count");
    }
    for (std::uint32_t i = 0; i < secretCount; ++i) {
        std::string secretId;
        if (!readString(file, secretId, kMaxStringLength)) {
            return bail("truncated or oversized secret id");
        }
        if (!secretId.empty()) {
            secrets.insert(secretId);
        }
    }

    std::uint32_t levelCount = 0;
    if (!readPod(file, levelCount)) {
        return bail("truncated level count");
    }
    if (levelCount > kMaxRecords) {
        return bail("implausible level count");
    }
    for (std::uint32_t i = 0; i < levelCount; ++i) {
        std::string levelId;
        if (!readString(file, levelId, kMaxStringLength)) {
            return bail("truncated or oversized level id");
        }

        LevelProgress progress;
        std::uint8_t played = 0;
        std::uint8_t completed = 0;
        if (!readPod(file, played) || !readPod(file, completed) ||
            !readPod(file, progress.bestTimeSeconds) || !readPod(file, progress.bestScore)) {
            return bail("truncated level record");
        }
        progress.played = (played != 0);
        progress.completed = (completed != 0);
        if (!levelId.empty()) {
            levels[levelId] = progress;
        }
    }

    m_unlockedSecrets.swap(secrets);
    m_levels.swap(levels);

    // Never trust a stored unlock flag - there isn't one. Recompute from the
    // registries every time the profile is loaded.
    recomputeBulletHell();

    HU_LOG_INFO(kLogCategory,
                "Loaded '%s' (v%u): %zu secret(s) unlocked of %zu, %zu level record(s), bullet hell %s",
                path.c_str(), version, secretsFoundTotal(), secretsTotal(),
                m_levels.size(), m_bulletHellUnlocked ? "UNLOCKED" : "locked");
    return true;
}

bool SaveGame::save(const std::string& path) const {
    m_lastError.clear();

    std::error_code ec;
    const std::filesystem::path filePath(path);
    if (filePath.has_parent_path()) {
        std::filesystem::create_directories(filePath.parent_path(), ec);
        if (ec) {
            m_lastError = "Failed to create save directory: " + ec.message();
            HU_LOG_WARN(kLogCategory, "%s", m_lastError.c_str());
            return false;
        }
    }

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        m_lastError = "Failed to open save file for writing: " + path;
        HU_LOG_WARN(kLogCategory, "%s", m_lastError.c_str());
        return false;
    }

    writePod(file, kMagic);
    writePod(file, kVersion);

    writePod(file, static_cast<std::uint32_t>(m_unlockedSecrets.size()));
    for (const std::string& secretId : m_unlockedSecrets) {
        writeString(file, secretId);
    }

    writePod(file, static_cast<std::uint32_t>(m_levels.size()));
    for (const auto& entry : m_levels) {
        writeString(file, entry.first);
        const std::uint8_t played = entry.second.played ? 1u : 0u;
        const std::uint8_t completed = entry.second.completed ? 1u : 0u;
        writePod(file, played);
        writePod(file, completed);
        writePod(file, entry.second.bestTimeSeconds);
        writePod(file, entry.second.bestScore);
    }

    file.flush();
    const bool good = file.good();
    file.close();

    if (!good) {
        m_lastError = "Error occurred while writing " + path;
        HU_LOG_WARN(kLogCategory, "%s", m_lastError.c_str());
        return false;
    }

    HU_LOG_INFO(kLogCategory, "Saved '%s' (v%u): %zu secret id(s), %zu level record(s)",
                path.c_str(), kVersion, m_unlockedSecrets.size(), m_levels.size());
    return true;
}

} // namespace hu
