#pragma once

#include <string>
#include <vector>
#include <functional>

enum class DebugWindow {
    None,
    PlayerStats,
    EnemyStats,
    SpawnerSettings
};

class DebugConsole {
public:
    static DebugConsole& getInstance() {
        static DebugConsole instance;
        return instance;
    }

    void processCommand(const std::string& command);
    void render();
    
    void toggleConsole() { m_showConsole = !m_showConsole; }
    void setVisible(bool visible) { m_showConsole = visible; }
    bool isVisible() const { return m_showConsole; }

private:
    DebugConsole() = default;
    ~DebugConsole() = default;
    DebugConsole(const DebugConsole&) = delete;
    DebugConsole& operator=(const DebugConsole&) = delete;

    void renderMainConsole();
    void renderPlayerStatsWindow();
    void renderEnemyStatsWindow();
    void renderSpawnerSettingsWindow();

    bool m_showConsole = false;
    DebugWindow m_currentWindow = DebugWindow::None;
    
    char m_commandBuffer[256] = {};
    std::vector<std::string> m_commandHistory;
};
