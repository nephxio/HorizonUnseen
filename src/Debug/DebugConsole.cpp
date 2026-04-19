#include "DebugConsole.h"
#include "Config/GameConfig.h"
#include "Config/ConfigManager.h"
#include "Game/GameScene.h"
#include "Game/HealthSystem.h"
#include <imgui.h>
#include <sstream>
#include <algorithm>

void DebugConsole::processCommand(const std::string& command) {
    if (command.empty()) return;

    // Add to history
    m_commandHistory.push_back(command);

    // Parse command
    std::istringstream iss(command);
    std::string baseCommand;
    iss >> baseCommand;

    // Convert to lowercase for case-insensitive comparison
    std::transform(baseCommand.begin(), baseCommand.end(), baseCommand.begin(), ::tolower);

    if (baseCommand == "debug") {
        std::string arg;
        iss >> arg;
        std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);

        if (arg == "playerstat" || arg == "playerstats") {
            m_currentWindow = DebugWindow::PlayerStats;
        }
        else if (arg == "enemystat" || arg == "enemystats") {
            m_currentWindow = DebugWindow::EnemyStats;
        }
        else if (arg == "spawner" || arg == "spawners") {
            m_currentWindow = DebugWindow::SpawnerSettings;
        }
        else if (arg == "gamestate" || arg == "state") {
            m_currentWindow = DebugWindow::GameState;
        }
        else if (arg == "close" || arg == "hide") {
            m_currentWindow = DebugWindow::None;
            m_showConsole = false;
        }
        else {
            // Show help
            m_currentWindow = DebugWindow::None;
        }
    }
}

void DebugConsole::render() {
    // Update status timer
    if (m_statusTimer > 0.0f) {
        m_statusTimer -= ImGui::GetIO().DeltaTime;
    }

    if (!m_showConsole) {
        // Still show status messages even if console is hidden
        if (m_statusTimer > 0.0f) {
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 50), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.8f);
            ImGui::Begin("##StatusMessage", nullptr, 
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | 
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);

            ImVec4 color = m_statusSuccess ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
            ImGui::TextColored(color, "%s", m_statusMessage.c_str());

            ImGui::End();
        }
        return;
    }

    renderMainConsole();

    // Render active debug window
    switch (m_currentWindow) {
        case DebugWindow::PlayerStats:
            renderPlayerStatsWindow();
            break;
        case DebugWindow::EnemyStats:
            renderEnemyStatsWindow();
            break;
        case DebugWindow::SpawnerSettings:
            renderSpawnerSettingsWindow();
            break;
        default:
            break;
    }

    // Show save/load status message if active
    if (m_statusTimer > 0.0f) {
        ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 50), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.8f);
        ImGui::Begin("##StatusMessage", nullptr, 
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | 
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);

        ImVec4 color = m_statusSuccess ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        ImGui::TextColored(color, "%s", m_statusMessage.c_str());

        ImGui::End();
    }
}

void DebugConsole::showSaveLoadStatus(const std::string& message, bool success) {
    m_statusMessage = message;
    m_statusSuccess = success;
    m_statusTimer = 3.0f;
}

void DebugConsole::renderMainConsole() {
    ImGui::SetNextWindowSize(ImVec2(500, 150), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Debug Console", &m_showConsole)) {
        ImGui::Text("Enter command (e.g., 'debug playerstat'):");
        ImGui::Separator();

        // Command input
        if (ImGui::InputText("##command", m_commandBuffer, sizeof(m_commandBuffer), 
            ImGuiInputTextFlags_EnterReturnsTrue)) {
            processCommand(m_commandBuffer);
            m_commandBuffer[0] = '\0';
            ImGui::SetKeyboardFocusHere(-1);
        }

        if (ImGui::Button("Execute")) {
            processCommand(m_commandBuffer);
            m_commandBuffer[0] = '\0';
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear History")) {
            m_commandHistory.clear();
        }

        // Help section
        ImGui::Separator();
        ImGui::Text("Available Commands:");
        ImGui::BulletText("debug playerstat - Player statistics");
        ImGui::BulletText("debug enemystats - Enemy statistics");
        ImGui::BulletText("debug spawners - Spawner settings");
        ImGui::BulletText("debug close - Close all windows");
    }
    ImGui::End();
}

void DebugConsole::renderPlayerStatsWindow() {
    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(520, 10), ImGuiCond_FirstUseEver);

    bool open = true;
    if (ImGui::Begin("Player Statistics", &open)) {
        auto& config = GameConfig::getInstance();

        ImGui::Text("Player Configuration");
        ImGui::Separator();

        // Hit Points
        ImGui::SliderFloat("Hit Points", &config.playerHitPoints, 1.0f, 1000.0f, "%.1f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Player's maximum health");
        }

        // Movement Speed X
        ImGui::SliderFloat("Movement Speed X", &config.playerMovementSpeedX, 50.0f, 500.0f, "%.1f px/s");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Horizontal movement speed");
        }

        // Movement Speed Y
        ImGui::SliderFloat("Movement Speed Y", &config.playerMovementSpeedY, 50.0f, 500.0f, "%.1f px/s");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Vertical movement speed");
        }

        ImGui::Separator();
        ImGui::Text("Weapon Configuration");

        // Fire Rate
        ImGui::SliderFloat("Fire Rate", &config.playerFireRate, 0.1f, 10.0f, "%.1f shots/sec");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Number of shots per second");
        }

        // Bullet Damage
        ImGui::SliderFloat("Bullet Damage", &config.playerBulletDamage, 1.0f, 100.0f, "%.1f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Damage dealt by each bullet");
        }

        // Bullet Speed
        ImGui::SliderFloat("Bullet Speed", &config.playerBulletSpeed, 100.0f, 1000.0f, "%.1f px/s");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Velocity of fired bullets");
        }

        ImGui::Separator();
        ImGui::Text("Note: Health changes apply to newly spawned player");

        ImGui::Separator();
        ImGui::Text("Health Cell Configuration");

        ImGui::SliderFloat("Cell Max Health", &config.healthCellMaxHealth, 10.0f, 500.0f, "%.1f");
        ImGui::SliderFloat("Cell Max Energy", &config.healthCellMaxEnergy, 10.0f, 500.0f, "%.1f");
        ImGui::SliderFloat("Energy Gain Rate", &config.healthCellEnergyGainRate, 0.1f, 5.0f, "%.2f");
        ImGui::SliderFloat("Decay Rate", &config.healthCellDecayRate, 1.0f, 50.0f, "%.1f");

        for (int i = 0; i < 5; ++i) {
            std::string label = "Threshold Cell " + std::to_string(i + 1);
            ImGui::SliderFloat(label.c_str(), &config.healthCellDamageThresholds[i], 0.0f, 200.0f, "%.1f dmg/s");
        }

        ImGui::Separator();

        // Reset buttons
        if (ImGui::Button("Reset to Default")) {
            config.playerHitPoints = 100.0f;
            config.playerMovementSpeedX = 200.0f;
            config.playerMovementSpeedY = 200.0f;
            config.playerFireRate = 1.0f;
            config.playerBulletDamage = 10.0f;
            config.playerBulletSpeed = 500.0f;
            config.healthCellMaxHealth = 100.0f;
            config.healthCellMaxEnergy = 100.0f;
            config.healthCellEnergyGainRate = 1.0f;
            config.healthCellDecayRate = 10.0f;
            for (int i = 0; i < 5; ++i) {
                config.healthCellDamageThresholds[i] = 25.0f;
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Close")) {
            open = false;
        }

        ImGui::Separator();
        ImGui::Text("Save/Load Configuration");

        if (ImGui::Button("Save Config to File")) {
            if (ConfigManager::saveConfig(config)) {
                showSaveLoadStatus("Configuration saved successfully!", true);
            } else {
                showSaveLoadStatus("Failed to save: " + ConfigManager::getLastError(), false);
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Load Config from File")) {
            if (ConfigManager::loadConfig(config)) {
                showSaveLoadStatus("Configuration loaded successfully!", true);
            } else {
                showSaveLoadStatus("Failed to load: " + ConfigManager::getLastError(), false);
            }
        }
    }
    ImGui::End();

    if (!open) {
        m_currentWindow = DebugWindow::None;
    }
}

void DebugConsole::renderEnemyStatsWindow() {
    ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(520, 10), ImGuiCond_FirstUseEver);

    bool open = true;
    if (ImGui::Begin("Enemy Statistics", &open)) {
        auto& config = GameConfig::getInstance();

        ImGui::Text("Enemy Configuration");
        ImGui::Separator();

        ImGui::SliderFloat("Hit Points", &config.enemyHitPoints, 1.0f, 100.0f, "%.1f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enemy health points");
        }

        ImGui::SliderFloat("Collision Damage", &config.enemyDamage, 1.0f, 100.0f, "%.1f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Damage dealt to player on collision");
        }

        ImGui::SliderFloat("Horizontal Speed", &config.enemyHorizontalSpeed, 50.0f, 400.0f, "%.1f px/s");
        ImGui::SliderFloat("Dive Speed", &config.enemyDiveSpeed, 100.0f, 600.0f, "%.1f px/s");
        ImGui::SliderFloat("Dive Angle", &config.enemyDiveAngle, 0.0f, 90.0f, "%.1f degrees");

        ImGui::Separator();
        ImGui::Text("Enemy Weapon Configuration");

        ImGui::SliderFloat("Fire Rate", &config.enemyFireRate, 0.1f, 5.0f, "%.1f shots/sec");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enemy shots per second");
        }

        ImGui::SliderFloat("Bullet Damage", &config.enemyBulletDamage, 1.0f, 50.0f, "%.1f");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Damage per enemy bullet");
        }

        ImGui::SliderFloat("Bullet Speed", &config.enemyBulletSpeed, 50.0f, 500.0f, "%.1f px/s");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enemy bullet velocity");
        }

        static int poolSize = config.enemyBulletPoolSize;
        ImGui::SliderInt("Bullet Pool Size", &poolSize, 50, 500);
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Maximum enemy bullets on screen\n(restart scene to apply)");
        }

        if (poolSize != config.enemyBulletPoolSize) {
            if (ImGui::Button("Apply Pool Size (Restart Scene)")) {
                config.enemyBulletPoolSize = poolSize;
            }
        }

        ImGui::Separator();
        ImGui::Text("Save/Load Configuration");

        if (ImGui::Button("Save Config to File")) {
            if (ConfigManager::saveConfig(config)) {
                showSaveLoadStatus("Configuration saved successfully!", true);
            } else {
                showSaveLoadStatus("Failed to save: " + ConfigManager::getLastError(), false);
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Load Config from File")) {
            if (ConfigManager::loadConfig(config)) {
                poolSize = config.enemyBulletPoolSize;
                showSaveLoadStatus("Configuration loaded successfully!", true);
            } else {
                showSaveLoadStatus("Failed to load: " + ConfigManager::getLastError(), false);
            }
        }

        ImGui::Separator();

        if (ImGui::Button("Reset to Default")) {
            config.enemyHitPoints = 10.0f;
            config.enemyDamage = 50.0f;
            config.enemyHorizontalSpeed = 150.0f;
            config.enemyDiveSpeed = 300.0f;
            config.enemyDiveAngle = 30.0f;
            config.enemyFireRate = 0.5f;
            config.enemyBulletDamage = 10.0f;
            config.enemyBulletSpeed = 200.0f;
            poolSize = 100;
        }

        ImGui::SameLine();
        if (ImGui::Button("Close")) {
            open = false;
        }
    }
    ImGui::End();

    if (!open) {
        m_currentWindow = DebugWindow::None;
    }
}

void DebugConsole::renderSpawnerSettingsWindow() {
    ImGui::SetNextWindowSize(ImVec2(400, 200), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(520, 10), ImGuiCond_FirstUseEver);

    bool open = true;
    if (ImGui::Begin("Spawner Settings", &open)) {
        auto& config = GameConfig::getInstance();

        ImGui::Text("Spawner Configuration");
    ImGui::Text("Spawner Configuration");
        ImGui::Separator();

        ImGui::SliderFloat("Base Interval", &config.spawnerBaseInterval, 0.5f, 10.0f, "%.1f seconds");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Base spawn interval for first spawner");
        }

        ImGui::SliderFloat("Interval Increment", &config.spawnerIntervalIncrement, 0.0f, 2.0f, "%.1f seconds");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Added to each subsequent spawner");
        }

        ImGui::Separator();

        if (ImGui::Button("Reset to Default")) {
            config.spawnerBaseInterval = 2.5f;
            config.spawnerIntervalIncrement = 0.5f;
        }

        ImGui::SameLine();
        if (ImGui::Button("Close")) {
            open = false;
        }

        ImGui::Separator();
        ImGui::Text("Save/Load Configuration");

        if (ImGui::Button("Save Config to File")) {
            if (ConfigManager::saveConfig(config)) {
                showSaveLoadStatus("Configuration saved successfully!", true);
            } else {
                showSaveLoadStatus("Failed to save: " + ConfigManager::getLastError(), false);
            }
        }

        ImGui::SameLine();

        if (ImGui::Button("Load Config from File")) {
            if (ConfigManager::loadConfig(config)) {
                showSaveLoadStatus("Configuration loaded successfully!", true);
            } else {
                showSaveLoadStatus("Failed to load: " + ConfigManager::getLastError(), false);
            }
        }
    }
    ImGui::End();

    if (!open) {
        m_currentWindow = DebugWindow::None;
    }
}

void DebugConsole::renderGameStateWindow() {
    ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(520, 10), ImGuiCond_FirstUseEver);

    bool open = true;
    if (ImGui::Begin("Game State", &open)) {
        ImGui::Text("Runtime Information");
        ImGui::Separator();

        ImGui::Text("This window shows live game data");
        ImGui::Text("Access via renderHUD() call");

        if (ImGui::Button("Close")) {
            open = false;
        }
    }
    ImGui::End();

    if (!open) {
        m_currentWindow = DebugWindow::None;
    }
}

void DebugConsole::renderHUD(const GameScene* scene) {
    if (!scene) return;

    // Always-visible HUD (not part of debug console)
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.5f);

    if (ImGui::Begin("HUD", nullptr, 
        ImGuiWindowFlags_NoTitleBar | 
        ImGuiWindowFlags_NoResize | 
        ImGuiWindowFlags_AlwaysAutoResize | 
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings)) {

        const auto& player = scene->getPlayer();

        // Health bar
        ImGui::Text("Health: %.0f / %.0f", player.getHealth(), player.getMaxHealth());

        float healthPercent = player.getHealth() / player.getMaxHealth();
        ImVec4 healthColor = healthPercent > 0.5f ? ImVec4(0.0f, 1.0f, 0.0f, 1.0f) : 
                             healthPercent > 0.25f ? ImVec4(1.0f, 1.0f, 0.0f, 1.0f) : 
                             ImVec4(1.0f, 0.0f, 0.0f, 1.0f);

        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, healthColor);
        ImGui::ProgressBar(healthPercent, ImVec2(200, 20), "");
        ImGui::PopStyleColor();

        // Enemy count
        ImGui::Text("Enemies: %zu", scene->getEnemies().size());

        // Collision count
        ImGui::Text("Collisions: %d", scene->getCollisionCount());

        // Bullet count
        ImGui::Text("Bullets: %zu", scene->getBullets().size());

        // Enemy bullet count
        ImGui::Text("Enemy Bullets: %zu / %zu", 
                    scene->getEnemyBulletPool().getActiveCount(),
                    scene->getEnemyBulletPool().getCapacity());

        // Player position
        ImGui::Text("Player: (%.0f, %.0f)", player.getPosition().x, player.getPosition().y);

        // Show first enemy position if exists
        if (!scene->getEnemies().empty()) {
            const auto& enemy = scene->getEnemies().front();
            ImGui::Text("Enemy: (%.0f, %.0f)", enemy->getPosition().x, enemy->getPosition().y);
        }

        // Player status
        if (!player.isAlive()) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "GAME OVER");
        }
    }
    ImGui::End();
}
