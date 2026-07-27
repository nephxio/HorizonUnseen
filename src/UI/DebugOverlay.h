#pragma once

#include "Core/Log.h"
#include "Game/Entity.h"   // Vector2
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

    Vector2 playerPosition{ 0.0f, 0.0f };
};

// Toggled with the backtick key. Shows a filterable view of the log ring buffer
// plus live counters, so a play session can be diagnosed without a debugger.
class DebugOverlay {
public:
    void draw(const DebugStats& stats, const HudModel& hud, bool& open);

    bool isLogPaused() const { return m_paused; }

private:
    void drawStatsPanel(const DebugStats& stats, const HudModel& hud);
    void drawLogPanel();

    char m_filter[128] = {};
    bool m_paused = false;
    bool m_autoScroll = true;
    int m_minLevel = static_cast<int>(LogLevel::Debug);
    std::deque<LogEntry> m_frozen;   // Snapshot held while paused.
};

} // namespace hu
