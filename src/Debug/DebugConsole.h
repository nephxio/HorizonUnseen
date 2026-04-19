#pragma once

#include <string>
#include <vector>
#include <functional>

class GameScene;
class HealthSystem;

enum class DebugWindow {
    None,
    PlayerStats,
    EnemyStats,
    SpawnerSettings,
    GameState
};

class DebugConsole {
public:
    static DebugConsole& getInstance() {
        static DebugConsole instance;
        return instance;
    }

    void processCommand(const std::string& command);
    void render();
    void renderHUD(const class GameScene* scene);

    void showSaveLoadStatus(const std::string& message, bool success);

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
    void renderGameStateWindow();

    bool m_showConsole = false;
    DebugWindow m_currentWindow = DebugWindow::None;

    char m_commandBuffer[256] = {};
    std::vector<std::string> m_commandHistory;

    std::string m_statusMessage;
    bool m_statusSuccess = true;
    float m_statusTimer = 0.0f;
};
