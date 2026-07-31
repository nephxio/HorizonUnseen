#pragma once

#include "Core/Log.h"
#include "Core/Vector2.h"
#include "UI/UiModel.h"

#include <string>
#include <vector>

namespace hu {

// Live counters the overlay displays. Filled by the scene each frame.
struct DebugStats {
    float fps = 0.0f;
    float frameTimeMs = 0.0f;

    int enemies = 0;
    int playerProjectiles = 0;
    int enemyProjectiles = 0;
    int particles = 0;
    int particleCapacity = 0;
    int powerups = 0;

    int drawInstances = 0;
    int drawBatches = 0;

    float levelTime = 0.0f;
    float scrollSpeed = 0.0f;

    // Energy cell internals, which are otherwise invisible to the player.
    float damageWindowRate = 0.0f;
    float activeThreshold = 0.0f;

    // Hitbox/graze geometry, so the tuning can be read at a glance.
    float hitboxRadius = 0.0f;
    float grazeRadius = 0.0f;
    long long grazeCount = 0;

    Vector2 playerPosition{ 0.0f, 0.0f };
};

// Actions the debug console can ask the application to perform.
//
// The overlay stays pure: it only records what was clicked and the application
// decides how to carry it out, exactly like the menus do.
struct DebugRequest {
    bool startNormal = false;
    bool startBulletHell = false;    // Bypasses the secret-gated menu button.
    bool unlockAllSecrets = false;
    bool resetProgress = false;

    bool fillCharge = false;
    bool repairAllCells = false;
    bool breakOneCell = false;
    bool grantAllWeapons = false;

    bool toggleInvulnerable = false;
    bool killAllEnemies = false;
    bool skipToBoss = false;
};

// Toggled with the backtick key. Shows a filterable view of the log ring buffer,
// live counters, and playtest shortcuts, so a session can be diagnosed and
// specific situations reached without a debugger or a grind.
class DebugOverlay {
public:
    // Any buttons pressed this frame are recorded into `request`.
    void draw(const DebugStats& stats, const HudModel& hud, bool& open, DebugRequest& request);

    bool isLogPaused() const { return m_paused; }

private:
    void drawStatsPanel(const DebugStats& stats, const HudModel& hud);
    void drawLogPanel();
    void drawCheatsPanel(const DebugStats& stats, DebugRequest& request);

    char m_filter[128] = {};
    bool m_paused = false;
    bool m_autoScroll = true;
    int m_minLevel = static_cast<int>(LogLevel::Debug);
    std::deque<LogEntry> m_frozen;   // Snapshot held while paused.
};

} // namespace hu
