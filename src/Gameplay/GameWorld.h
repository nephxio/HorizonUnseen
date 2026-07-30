#pragma once

// The playable scene.
//
// GameWorld owns every gameplay system and is the concrete implementation of
// IGameWorld that they all talk through. It deliberately knows nothing about
// Vulkan or ImGui: it consumes input, advances simulation, and emits a
// renderer-agnostic DrawList plus a handful of plain readouts the application
// turns into HUD/menu state.

#include "Core/DrawList.h"
#include "Core/GameTypes.h"
#include "Gameplay/Enemies/EnemyBase.h"
#include "Gameplay/IGameWorld.h"
#include "Gameplay/Levels/LevelDirector.h"
#include "Gameplay/Particles/ParticleSystem.h"
#include "Gameplay/PlayerCommand.h"
#include "Gameplay/Particles/Starfield.h"
#include "Gameplay/Power/EnergyCellSystem.h"
#include "Gameplay/Power/PowerupSystem.h"
#include "Gameplay/Projectiles/ProjectilePool.h"
#include "Gameplay/Secrets/SecretTracker.h"
#include "Gameplay/Weapons/Superweapons.h"
#include "Gameplay/Weapons/WeaponSystem.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace hu {

// The player's ship. Small enough that it does not warrant its own file.
struct PlayerShip {
    Vector2 position{ 220.0f, 360.0f };
    Vector2 velocity{ 0.0f, 0.0f };
    // Collision radius. Bullet hell shrinks this dramatically (see
    // GameWorld::hitboxRadius) so dense patterns stay threadable.
    float radius = 15.0f;
    float hitFlash = 0.0f;
    // Brief mercy invulnerability after a cell breaks, so one bad moment does
    // not cascade into losing the whole stack.
    float invulnerable = 0.0f;
    bool alive = true;
};

// A gameplay notification the UI should surface. Kept UI-agnostic so the
// gameplay library does not depend on the UI library.
enum class NoticeKind : std::uint8_t {
    Info = 0,
    Secret,
    Powerup,
    Warning
};

struct Notice {
    std::string title;
    std::string subtitle;
    NoticeKind kind = NoticeKind::Info;
};

class GameWorld : public IGameWorld {
public:
    GameWorld();
    ~GameWorld() override;

    // --- Session ------------------------------------------------------------
    // Returns false when the level id is not registered.
    bool startLevel(const std::string& levelId, DifficultyMode mode);
    void restart();

    // The simulation is driven by intent, not by hardware: see PlayerCommand.h.
    void update(float deltaTime, const PlayerCommand& command);

    // Emits the whole scene, sorted by layer and ready to hand to the renderer.
    void buildDrawList(DrawList& out) const;

    // --- Readouts for the application/UI ------------------------------------
    const PlayerShip& player() const { return m_player; }
    const EnergyCellSystem& cells() const { return m_cells; }
    const WeaponSystem& weapons() const { return m_weapons; }
    const SuperweaponSystem& superweapons() const { return m_super; }
    const LevelDirector& director() const { return m_director; }
    const SecretTracker& secrets() const { return m_secrets; }
    const ParticleSystem& particles() const { return m_particles; }
    const ProjectilePool& projectiles() const { return m_projectiles; }
    const PowerupSystem& powerups() const { return m_powerups; }

    const std::string& levelId() const { return m_levelId; }
    const std::string& levelDisplayName() const { return m_levelDisplayName; }
    long long score() const { return m_score; }
    std::size_t enemyCount() const { return m_enemies.size(); }

    // Near-misses this run, and the band they are counted in. Surfaced so the
    // HUD can show grazing actually paying out.
    long long grazeCount() const { return m_grazeCount; }
    float hitboxRadius() const;
    float grazeRadius() const;

    bool playerDead() const { return !m_player.alive; }
    bool levelComplete() const { return m_director.isComplete(); }

    // Boss readout for the HUD; returns false when no boss is on screen.
    bool bossStatus(float& healthFraction) const;

    // Drains queued notifications (secrets found, pick-ups, cell events).
    std::vector<Notice> takeNotices();

    // Camera shake offset applied by the renderer/draw list this frame.
    Vector2 shakeOffset() const { return m_shakeOffset; }

    // --- Debug / playtest hooks --------------------------------------------
    // Driven by the debug console. These deliberately bypass normal progression
    // so a specific situation can be reached without grinding to it.
    void debugFillCharge();
    void debugRepairAllCells();
    void debugBreakOneCell();
    void debugGrantAllWeapons();
    void debugKillAllEnemies();
    void debugSkipToBoss();
    void debugToggleInvulnerable();
    bool debugInvulnerable() const { return m_debugInvulnerable; }

    // --- IGameWorld ---------------------------------------------------------
    void spawnPlayerProjectile(const ProjectileSpawn& spawn) override;
    void spawnEnemyProjectile(const ProjectileSpawn& spawn) override;
    void spawnPowerup(PowerupType type, Vector2 position) override;
    void spawnEffect(const EffectRequest& request) override;

    Vector2 playerPosition() const override;
    bool findNearestEnemy(Vector2 from, float maxDistance, TargetInfo& out) const override;
    std::vector<TargetInfo> findEnemies(Vector2 from, float maxDistance, int maxCount) const override;
    bool resolveTarget(std::uint32_t handle, TargetInfo& out) const override;

    void clearScreen(float bossDamage) override;
    void addScreenShake(float intensity, float duration) override;

    DifficultyMode difficulty() const override { return m_difficulty; }
    float elapsedLevelTime() const override { return m_levelTime; }
    float screenWidth() const override { return m_screenWidth; }
    float screenHeight() const override { return m_screenHeight; }

private:
    void applyCommand(float deltaTime, const PlayerCommand& command);
    void updateEnemies(float deltaTime);
    void resolveEnemyDeaths();
    void updateCollisions(float deltaTime);
    void updateGrazing(float deltaTime);
    void applyBeamDamage(float deltaTime);
    void collectPowerups();
    void damagePlayer(float amount, Vector2 source);
    void pushNotice(const std::string& title, const std::string& subtitle, NoticeKind kind);
    void updateShake(float deltaTime);
    static TargetInfo describe(const EnemyBase& enemy);

    // --- Systems ------------------------------------------------------------
    PlayerShip m_player;
    EnergyCellSystem m_cells;
    WeaponSystem m_weapons;
    SuperweaponSystem m_super;
    ProjectilePool m_projectiles;
    PowerupSystem m_powerups;
    ParticleSystem m_particles;
    Starfield m_starfield;
    LevelDirector m_director;
    SecretTracker m_secrets;

    std::vector<std::unique_ptr<EnemyBase>> m_enemies;

    // --- Session state ------------------------------------------------------
    std::string m_levelId;
    std::string m_levelDisplayName;
    DifficultyMode m_difficulty = DifficultyMode::Normal;
    float m_levelTime = 0.0f;
    long long m_score = 0;

    float m_screenWidth = 1280.0f;
    float m_screenHeight = 720.0f;

    std::uint32_t m_nextHandle = 1;   // 0 is reserved as InvalidTarget.

    // Cell count observed last frame, so a break can be detected and announced.
    std::size_t m_lastBrokenCells = 0;

    float m_shakeIntensity = 0.0f;
    float m_shakeTimer = 0.0f;
    float m_shakeDuration = 0.0f;
    Vector2 m_shakeOffset{ 0.0f, 0.0f };

    float m_thrusterTimer = 0.0f;
    bool m_bossDefeatReported = false;

    bool m_debugInvulnerable = false;

    long long m_grazeCount = 0;
    // Graze effects are throttled: in bullet hell hundreds of near-misses can
    // land in a second and one burst each would swamp the particle pool.
    float m_grazeEffectTimer = 0.0f;

    std::vector<Notice> m_notices;
};

} // namespace hu
