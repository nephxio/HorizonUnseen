#include "Application.h"

#include "Core/Log.h"
#include "Core/SaveGame.h"
#include "Gameplay/Levels/Levels.h"
#include "Gameplay/Secrets/SecretRegistry.h"
#include "Systems/InputSystem.h"
#include "UI/UiTheme.h"

#include <GLFW/glfw3.h>
#include <imgui.h>

#include <algorithm>

namespace {

constexpr const char* LogCat = "App";

// The level "Start Game" launches. The registry is the source of truth; this is
// only the entry point into it.
constexpr const char* StartingLevelId = "test_level";

hu::ToastKind toToastKind(hu::NoticeKind kind) {
    switch (kind) {
        case hu::NoticeKind::Secret:  return hu::ToastKind::Secret;
        case hu::NoticeKind::Powerup: return hu::ToastKind::Powerup;
        case hu::NoticeKind::Warning: return hu::ToastKind::Warning;
        default:                      return hu::ToastKind::Info;
    }
}

} // namespace

Application::Application() {
    init();
}

Application::~Application() {
    cleanup();
}

void Application::init() {
    hu::Log::init("logs");
    HU_LOG_INFO(LogCat, "Horizon Unseen starting up");

    m_renderer = std::make_unique<VulkanRenderer>();
    m_renderer->init();

    m_input = std::make_unique<InputSystem>(m_renderer->getWindow());
    m_world = std::make_unique<hu::GameWorld>();

    hu::theme::apply();

    // Progression is loaded once; a missing file just means a fresh profile.
    hu::SaveGame::instance().load();

    HU_LOG_INFO(LogCat, "%zu level(s) registered, %zu secret(s) total, bullet hell %s",
                hu::LevelRegistry::count(),
                hu::SecretRegistry::totalCount(),
                hu::SaveGame::instance().bulletHellUnlocked() ? "UNLOCKED" : "locked");

    buildProgressModel();

    m_renderer->setUiCallback([this]() { drawUi(); });

    m_drawList.reserve(4096);
    m_running = true;
}

void Application::run() {
    while (m_running && !m_renderer->shouldClose()) {
        const float deltaTime = m_frameTimer.tick();
        m_lastDeltaTime = deltaTime;

        hu::Log::beginFrame(m_frameIndex++, hu::Log::currentTime() + deltaTime);

        update(deltaTime);

        m_renderer->setDrawList(&m_drawList);
        m_renderer->renderFrame();
    }

    m_renderer->waitIdle();
    HU_LOG_INFO(LogCat, "Main loop exited after %llu frames", m_frameIndex);
}

void Application::update(float deltaTime) {
    m_input->update();

    // FPS smoothing for the debug overlay.
    m_fpsAccumulator += deltaTime;
    ++m_fpsSamples;
    if (m_fpsAccumulator >= 0.25f) {
        m_displayFps = static_cast<float>(m_fpsSamples) / m_fpsAccumulator;
        m_fpsAccumulator = 0.0f;
        m_fpsSamples = 0;
    }

    if (m_input->isKeyJustPressed(GLFW_KEY_GRAVE_ACCENT)) {
        m_debugOpen = !m_debugOpen;
        HU_LOG_DEBUG(LogCat, "Debug console %s", m_debugOpen ? "opened" : "closed");
    }

    switch (m_state) {
        case hu::GameStateId::Playing: {
            if (m_input->isKeyJustPressed(GLFW_KEY_ESCAPE)) {
                m_state = hu::GameStateId::Paused;
                HU_LOG_INFO(LogCat, "Paused");
                break;
            }

            m_world->update(deltaTime, *m_input);

            // Surface gameplay notifications as HUD toasts.
            for (const hu::Notice& notice : m_world->takeNotices()) {
                hu::Toast toast;
                toast.title = notice.title;
                toast.subtitle = notice.subtitle;
                toast.kind = toToastKind(notice.kind);
                m_hud.pushToast(toast);
            }

            if (m_world->playerDead()) {
                m_state = hu::GameStateId::GameOver;
                hu::SaveGame::instance().save();
                buildProgressModel();
                HU_LOG_INFO(LogCat, "Game over");
            } else if (m_world->levelComplete()) {
                m_state = hu::GameStateId::LevelComplete;
                buildProgressModel();
                HU_LOG_INFO(LogCat, "Level complete");
            }
            break;
        }

        case hu::GameStateId::Paused:
            if (m_input->isKeyJustPressed(GLFW_KEY_ESCAPE)) {
                m_state = hu::GameStateId::Playing;
                HU_LOG_INFO(LogCat, "Resumed");
            }
            break;

        default:
            break;
    }

    // The world keeps drawing behind the pause/game-over/complete overlays.
    const bool worldVisible = m_state == hu::GameStateId::Playing ||
                              m_state == hu::GameStateId::Paused ||
                              m_state == hu::GameStateId::GameOver ||
                              m_state == hu::GameStateId::LevelComplete;

    if (worldVisible) {
        m_world->buildDrawList(m_drawList);
        buildHudModel();
        buildDebugStats();
    } else {
        m_drawList.clear();
    }
}

// ---------------------------------------------------------------------------
// UI
// ---------------------------------------------------------------------------

void Application::drawUi() {
    const float width = m_renderer->getViewportWidth();
    const float height = m_renderer->getViewportHeight();

    hu::MenuAction action = hu::MenuAction::None;

    switch (m_state) {
        case hu::GameStateId::MainMenu:
            action = m_menus.drawMainMenu(m_progressModel, width, height, m_lastDeltaTime);
            break;

        case hu::GameStateId::Playing:
            m_hud.draw(m_hudModel, width, height, m_lastDeltaTime);
            break;

        case hu::GameStateId::Paused:
            m_hud.draw(m_hudModel, width, height, 0.0f);
            action = m_menus.drawPauseMenu(width, height);
            break;

        case hu::GameStateId::GameOver:
            m_hud.draw(m_hudModel, width, height, 0.0f);
            action = m_menus.drawGameOver(m_hudModel, width, height);
            break;

        case hu::GameStateId::LevelComplete:
            m_hud.draw(m_hudModel, width, height, 0.0f);
            action = m_menus.drawLevelComplete(m_hudModel, width, height);
            break;

        case hu::GameStateId::SecretsScreen:
            action = m_menus.drawSecretsScreen(m_progressModel, width, height);
            break;

        case hu::GameStateId::Options:
            action = m_menus.drawOptions(width, height);
            break;
    }

    applyMenuAction(action);

    hu::DebugRequest debugRequest;
    m_debugOverlay.draw(m_debugStats, m_hudModel, m_debugOpen, debugRequest);
    applyDebugRequest(debugRequest);
}

void Application::applyDebugRequest(const hu::DebugRequest& request) {
    // Mode switches restart the level, so handle them first and return: the
    // remaining cheats would be acting on a world that is about to be replaced.
    if (request.startBulletHell) {
        HU_LOG_INFO(LogCat, "DEBUG: restarting in bullet hell");
        startLevel(hu::DifficultyMode::BulletHell);
        return;
    }
    if (request.startNormal) {
        HU_LOG_INFO(LogCat, "DEBUG: restarting in normal");
        startLevel(hu::DifficultyMode::Normal);
        return;
    }

    if (request.unlockAllSecrets) {
        // Driven off the registry, so new levels' secrets are covered
        // automatically.
        std::size_t unlocked = 0;
        for (const hu::SecretDefinition& secret : hu::SecretRegistry::all()) {
            if (hu::SaveGame::instance().unlockSecret(secret.id)) {
                ++unlocked;
            }
        }
        hu::SaveGame::instance().save();
        buildProgressModel();
        HU_LOG_INFO(LogCat, "DEBUG: unlocked %zu secret(s); bullet hell %s",
                    unlocked,
                    hu::SaveGame::instance().bulletHellUnlocked() ? "UNLOCKED" : "still locked");
    }

    if (request.resetProgress) {
        hu::SaveGame::instance().resetProgress();
        hu::SaveGame::instance().save();
        buildProgressModel();
        HU_LOG_INFO(LogCat, "DEBUG: progress reset");
    }

    if (request.fillCharge)        { m_world->debugFillCharge(); }
    if (request.repairAllCells)    { m_world->debugRepairAllCells(); }
    if (request.breakOneCell)      { m_world->debugBreakOneCell(); }
    if (request.grantAllWeapons)   { m_world->debugGrantAllWeapons(); }
    if (request.killAllEnemies)    { m_world->debugKillAllEnemies(); }
    if (request.skipToBoss)        { m_world->debugSkipToBoss(); }
    if (request.toggleInvulnerable){ m_world->debugToggleInvulnerable(); }
}

void Application::applyMenuAction(hu::MenuAction action) {
    switch (action) {
        case hu::MenuAction::StartNormal:
            startLevel(hu::DifficultyMode::Normal);
            break;

        case hu::MenuAction::StartBulletHell:
            startLevel(hu::DifficultyMode::BulletHell);
            break;

        case hu::MenuAction::OpenSecrets:
            buildProgressModel();
            m_state = hu::GameStateId::SecretsScreen;
            break;

        case hu::MenuAction::OpenOptions:
            m_state = hu::GameStateId::Options;
            break;

        case hu::MenuAction::BackToMainMenu:
            buildProgressModel();
            m_state = hu::GameStateId::MainMenu;
            break;

        case hu::MenuAction::Resume:
            m_state = hu::GameStateId::Playing;
            break;

        case hu::MenuAction::RestartLevel:
            startLevel(m_difficulty);
            break;

        case hu::MenuAction::QuitToMenu:
            returnToMenu();
            break;

        case hu::MenuAction::QuitGame:
            HU_LOG_INFO(LogCat, "Quit requested from menu");
            m_running = false;
            break;

        case hu::MenuAction::None:
        default:
            break;
    }
}

void Application::startLevel(hu::DifficultyMode mode) {
    m_difficulty = mode;
    m_hud.clearToasts();
    m_hudModel.bossName.clear();

    if (!m_world->startLevel(StartingLevelId, mode)) {
        HU_LOG_ERROR(LogCat, "Could not start level '%s'; returning to menu", StartingLevelId);
        returnToMenu();
        return;
    }

    m_state = hu::GameStateId::Playing;
    HU_LOG_INFO(LogCat, "Entering play: %s (%s)",
                StartingLevelId,
                mode == hu::DifficultyMode::BulletHell ? "BULLET HELL" : "Normal");
}

void Application::returnToMenu() {
    hu::SaveGame::instance().save();
    buildProgressModel();
    m_hud.clearToasts();
    m_state = hu::GameStateId::MainMenu;
    HU_LOG_INFO(LogCat, "Returned to main menu");
}

// ---------------------------------------------------------------------------
// View models
// ---------------------------------------------------------------------------

void Application::buildHudModel() {
    const hu::EnergyCellSystem& cells = m_world->cells();
    const hu::WeaponSystem& weapons = m_world->weapons();

    m_hudModel.cells.clear();
    for (std::size_t i = 0; i < hu::EnergyCellSystem::CellCount; ++i) {
        const hu::EnergyCell& source = cells.cell(i);
        hu::CellView view;
        view.health = source.health;
        view.maxHealth = source.maxHealth;
        view.charge = source.charge;
        view.maxCharge = source.maxCharge;
        view.broken = source.broken;
        view.charged = source.isCharged();
        m_hudModel.cells.push_back(view);
    }

    m_hudModel.weapons.clear();
    for (std::size_t i = 0; i < hu::WeaponTypeCount; ++i) {
        const hu::WeaponType type = static_cast<hu::WeaponType>(i);
        hu::WeaponView view;
        view.type = type;
        view.level = weapons.level(type);
        view.unlocked = weapons.unlocked(type);
        m_hudModel.weapons.push_back(view);
    }

    m_hudModel.currentWeapon = weapons.current();
    m_hudModel.chargedCells = cells.chargedCellCount();
    m_hudModel.pendingSuperweapon = hu::superweaponForCharge(m_hudModel.chargedCells);

    m_hudModel.damageRate = cells.windowedDamageRate();
    // Show the threshold of the cell currently deciding the routing.
    m_hudModel.damageThreshold = 0.0f;
    for (std::size_t i = 0; i < hu::EnergyCellSystem::CellCount; ++i) {
        const hu::EnergyCell& c = cells.cell(i);
        if (!c.broken && c.charge < c.maxCharge) {
            m_hudModel.damageThreshold = c.damageRateThreshold;
            break;
        }
    }

    m_hudModel.levelProgress01 = m_world->director().progress01();
    m_hudModel.levelName = m_world->levelDisplayName();
    m_hudModel.score = m_world->score();
    m_hudModel.difficulty = m_difficulty;

    const std::string& levelId = m_world->levelId();
    m_hudModel.secretsFound = static_cast<int>(hu::SaveGame::instance().secretsFoundInLevel(levelId));
    m_hudModel.secretsTotal = static_cast<int>(hu::SaveGame::instance().secretsTotalInLevel(levelId));
    m_hudModel.grazeCount = m_world->grazeCount();

    float bossHealth = 0.0f;
    m_hudModel.bossActive = m_world->bossStatus(bossHealth);
    m_hudModel.bossHealth01 = bossHealth;
    if (m_hudModel.bossActive && m_hudModel.bossName.empty()) {
        m_hudModel.bossName = "WARDEN";
    }
}

void Application::buildProgressModel() {
    const hu::SaveGame& save = hu::SaveGame::instance();

    m_progressModel.levels.clear();
    m_progressModel.totalFound = static_cast<int>(save.secretsFoundTotal());
    m_progressModel.totalSecrets = static_cast<int>(save.secretsTotal());
    m_progressModel.bulletHellUnlocked = save.bulletHellUnlocked();

    // Driven entirely off the registries, so a new level appears here with no
    // change to this function.
    for (const hu::LevelDefinition& level : hu::LevelRegistry::all()) {
        hu::LevelProgressView view;
        view.levelId = level.id;
        view.displayName = level.displayName;

        if (const hu::LevelProgress* progress = save.levelProgress(level.id)) {
            view.played = progress->played;
            view.completed = progress->completed;
        }

        view.secretsFound = static_cast<int>(save.secretsFoundInLevel(level.id));
        view.secretsTotal = static_cast<int>(save.secretsTotalInLevel(level.id));

        for (const hu::SecretDefinition* secret : hu::SecretRegistry::forLevel(level.id)) {
            if (!secret) {
                continue;
            }
            view.secretNames.push_back(secret->displayName);
            view.secretHints.push_back(secret->hint);
            view.secretUnlocked.push_back(save.isSecretUnlocked(secret->id));
        }

        m_progressModel.levels.push_back(std::move(view));
    }
}

void Application::buildDebugStats() {
    m_debugStats.fps = m_displayFps;
    m_debugStats.frameTimeMs = m_lastDeltaTime * 1000.0f;

    m_debugStats.enemies = static_cast<int>(m_world->enemyCount());
    m_debugStats.playerProjectiles = static_cast<int>(m_world->projectiles().activePlayerCount());
    m_debugStats.enemyProjectiles = static_cast<int>(m_world->projectiles().activeEnemyCount());
    m_debugStats.particles = static_cast<int>(m_world->particles().liveCount());
    m_debugStats.particleCapacity = static_cast<int>(m_world->particles().capacity());
    m_debugStats.powerups = static_cast<int>(m_world->powerups().activeCount());

    m_debugStats.drawInstances = static_cast<int>(m_drawList.size());

    m_debugStats.levelTime = m_world->director().elapsedTime();
    m_debugStats.scrollSpeed = m_world->director().scrollSpeed();

    m_debugStats.damageWindowRate = m_world->cells().windowedDamageRate();
    m_debugStats.activeThreshold = m_hudModel.damageThreshold;
    m_debugStats.playerPosition = m_world->player().position;

    m_debugStats.hitboxRadius = m_world->hitboxRadius();
    m_debugStats.grazeRadius = m_world->grazeRadius();
    m_debugStats.grazeCount = m_world->grazeCount();
}

void Application::cleanup() {
    if (m_renderer) {
        m_renderer->cleanup();
    }
    hu::Log::shutdown();
}
