#include "Gameplay/GameWorld.h"

#include "Config/GameConfig.h"
#include "Core/Log.h"
#include "Core/Math.h"
#include "Core/SaveGame.h"
#include "Gameplay/Levels/Levels.h"
#include "Gameplay/Particles/EffectLibrary.h"
#include "Gameplay/Secrets/SecretRegistry.h"
#include "Systems/InputSystem.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

namespace hu {
namespace {

// ---------------------------------------------------------------------------
// Tuning constants (this file's single tunable block)
// ---------------------------------------------------------------------------

// How far from the edges the ship is allowed to sit.
constexpr float PlayFieldMarginX = 24.0f;
constexpr float PlayFieldMarginY = 24.0f;

// Mercy window after a cell breaks.
constexpr float PlayerInvulnerabilityOnBreak = 1.1f;

// Contact damage is applied on a cadence rather than per frame so brushing an
// enemy is survivable but sitting inside one is not.
constexpr float ContactDamageInterval = 0.35f;

// Thruster particles are emitted on a timer, not every frame.
constexpr float ThrusterInterval = 0.02f;

constexpr float PlayerHitFlashDuration = 0.1f;

// Energy Bomb damage dealt to bosses.
constexpr float EnergyBombBossDamage = 900.0f;

// Score awarded for finding a secret.
constexpr long long SecretScore = 2500;

constexpr const char* LogCat = "World";

// Shortest distance from point p to the segment a..b. Used for beam hits.
float distanceToSegment(Vector2 p, Vector2 a, Vector2 b) {
    const Vector2 ab = sub(b, a);
    const float lenSq = lengthSquared(ab);
    if (lenSq <= 1e-6f) {
        return distance(p, a);
    }
    float t = dot(sub(p, a), ab) / lenSq;
    t = clampf(t, 0.0f, 1.0f);
    return distance(p, add(a, scale(ab, t)));
}

} // namespace

// ---------------------------------------------------------------------------
// Construction / session
// ---------------------------------------------------------------------------

GameWorld::GameWorld()
    : m_starfield(GameConfig::getInstance().screenWidth,
                  GameConfig::getInstance().screenHeight) {
    m_screenWidth = GameConfig::getInstance().screenWidth;
    m_screenHeight = GameConfig::getInstance().screenHeight;

    // The director hands finished enemies straight into our live list.
    m_director.setSpawnCallback([this](std::unique_ptr<EnemyBase> enemy) {
        if (!enemy) {
            return;
        }
        enemy->setHandle(m_nextHandle++);
        m_enemies.push_back(std::move(enemy));
    });
}

GameWorld::~GameWorld() = default;

bool GameWorld::startLevel(const std::string& levelId, DifficultyMode mode) {
    const LevelDefinition* level = LevelRegistry::find(levelId);
    if (!level) {
        HU_LOG_ERROR(LogCat, "startLevel: unknown level id '%s'", levelId.c_str());
        return false;
    }

    m_levelId = level->id;
    m_levelDisplayName = level->displayName;
    m_difficulty = mode;

    m_player = PlayerShip{};
    m_cells.reset();
    m_weapons.reset();
    m_super.reset();
    m_projectiles.reset();
    m_powerups.reset();
    m_particles.clear();
    m_enemies.clear();
    m_notices.clear();

    m_director.setDifficulty(mode);
    m_director.setLevel(level);
    m_director.reset();

    m_secrets.onLevelStart(m_levelId);

    m_levelTime = 0.0f;
    m_score = 0;
    m_nextHandle = 1;
    m_lastBrokenCells = 0;
    m_shakeIntensity = 0.0f;
    m_shakeTimer = 0.0f;
    m_shakeOffset = { 0.0f, 0.0f };
    m_bossDefeatReported = false;

    SaveGame::instance().markLevelPlayed(m_levelId);

    HU_LOG_INFO(LogCat, "Level '%s' started (%s), %zu secrets tracked",
                m_levelId.c_str(),
                mode == DifficultyMode::BulletHell ? "BULLET HELL" : "Normal",
                m_secrets.trackedCount());
    return true;
}

void GameWorld::restart() {
    if (!m_levelId.empty()) {
        startLevel(m_levelId, m_difficulty);
    }
}

// ---------------------------------------------------------------------------
// Frame
// ---------------------------------------------------------------------------

void GameWorld::update(float deltaTime, const InputSystem& input) {
    // A huge delta (breakpoint, window drag) would tunnel collisions and blow
    // up the damage window, so clamp it.
    deltaTime = std::min(deltaTime, 0.05f);

    m_levelTime += deltaTime;

    if (m_player.alive) {
        handleInput(deltaTime, input);
    } else {
        m_weapons.setFiring(false);
    }

    m_player.hitFlash = std::max(0.0f, m_player.hitFlash - deltaTime);
    m_player.invulnerable = std::max(0.0f, m_player.invulnerable - deltaTime);

    m_cells.update(deltaTime, m_levelTime);
    m_weapons.update(deltaTime, m_player.position, *this);
    m_super.update(deltaTime, m_player.position, *this, m_weapons, m_cells);

    m_director.update(deltaTime);

    updateEnemies(deltaTime);
    m_projectiles.update(deltaTime, *this);

    updateCollisions(deltaTime);
    applyBeamDamage(deltaTime);

    resolveEnemyDeaths();

    m_powerups.update(deltaTime, m_player.position, m_director.scrollSpeed(), *this);
    collectPowerups();

    m_particles.update(deltaTime);
    m_starfield.update(deltaTime, m_director.scrollSpeed());

    m_secrets.onPlayerMoved(m_player.position, m_levelTime);
    m_secrets.update(deltaTime, m_levelTime);

    // Surface any secrets that completed this frame.
    for (const SecretDefinition* secret : m_secrets.takeNewlyUnlocked()) {
        if (!secret) {
            continue;
        }
        SaveGame::instance().unlockSecret(secret->id);
        m_score += SecretScore;
        pushNotice("SECRET FOUND", secret->displayName, NoticeKind::Secret);

        EffectRequest fx;
        fx.kind = EffectKind::PowerupPickup;
        fx.position = m_player.position;
        fx.scale = 2.0f;
        fx.tint = Color{ 0.85f, 0.55f, 1.0f, 1.0f };
        spawnEffect(fx);

        HU_LOG_INFO(LogCat, "Secret unlocked: %s", secret->id.c_str());
    }

    // Announce cell breaks and grant a mercy window.
    const std::size_t broken = m_cells.brokenCellCount();
    if (broken > m_lastBrokenCells) {
        m_player.invulnerable = PlayerInvulnerabilityOnBreak;
        addScreenShake(9.0f, 0.35f);

        EffectRequest fx;
        fx.kind = EffectKind::CellBreak;
        fx.position = m_player.position;
        fx.scale = 1.4f;
        spawnEffect(fx);

        pushNotice("CELL BREACH", "Energy cell offline", NoticeKind::Warning);
    }
    m_lastBrokenCells = broken;

    if (m_player.alive && !m_cells.isAlive()) {
        m_player.alive = false;
        EffectRequest fx;
        fx.kind = EffectKind::BigExplosion;
        fx.position = m_player.position;
        fx.scale = 1.8f;
        spawnEffect(fx);
        addScreenShake(18.0f, 0.9f);
        HU_LOG_INFO(LogCat, "Player destroyed at t=%.2f, score %lld", m_levelTime, m_score);
    }

    // Record completion once, the first time the level reports done.
    if (m_director.isComplete() && !m_bossDefeatReported) {
        m_bossDefeatReported = true;
        SaveGame::instance().markLevelCompleted(m_levelId, m_levelTime,
                                                static_cast<std::int32_t>(m_score));
        SaveGame::instance().save();
        HU_LOG_INFO(LogCat, "Level complete in %.1fs, score %lld", m_levelTime, m_score);
    }

    updateShake(deltaTime);
}

void GameWorld::handleInput(float deltaTime, const InputSystem& input) {
    const GameConfig& cfg = GameConfig::getInstance();

    Vector2 move{ 0.0f, 0.0f };
    if (input.isKeyPressed(GLFW_KEY_A) || input.isKeyPressed(GLFW_KEY_LEFT))  { move.x -= 1.0f; }
    if (input.isKeyPressed(GLFW_KEY_D) || input.isKeyPressed(GLFW_KEY_RIGHT)) { move.x += 1.0f; }
    // World space is y-DOWN (see the projection in shaders/sprite.vert), so
    // moving up the screen means decreasing y.
    if (input.isKeyPressed(GLFW_KEY_W) || input.isKeyPressed(GLFW_KEY_UP))    { move.y -= 1.0f; }
    if (input.isKeyPressed(GLFW_KEY_S) || input.isKeyPressed(GLFW_KEY_DOWN))  { move.y += 1.0f; }

    if (lengthSquared(move) > 0.0f) {
        move = normalize(move);
    }

    m_player.velocity = { move.x * cfg.playerMovementSpeedX,
                          move.y * cfg.playerMovementSpeedY };
    m_player.position = add(m_player.position, scale(m_player.velocity, deltaTime));

    m_player.position.x = clampf(m_player.position.x, PlayFieldMarginX, m_screenWidth - PlayFieldMarginX);
    m_player.position.y = clampf(m_player.position.y, PlayFieldMarginY, m_screenHeight - PlayFieldMarginY);

    // Thruster plume while moving.
    m_thrusterTimer -= deltaTime;
    if (lengthSquared(move) > 0.0f && m_thrusterTimer <= 0.0f) {
        m_thrusterTimer = ThrusterInterval;
        EffectRequest fx;
        fx.kind = EffectKind::Thruster;
        fx.position = { m_player.position.x - 26.0f, m_player.position.y };
        fx.direction = { 1.0f, 0.0f };
        spawnEffect(fx);
    }

    const bool firing = input.isKeyPressed(GLFW_KEY_SPACE);
    const bool wasFiring = m_weapons.firing();
    m_weapons.setFiring(firing);
    if (firing && !wasFiring) {
        m_secrets.onPlayerFired(m_levelTime);
    }

    if (input.isKeyJustPressed(GLFW_KEY_LEFT_BRACKET))  { m_weapons.cycleWeapon(-1); }
    if (input.isKeyJustPressed(GLFW_KEY_RIGHT_BRACKET)) { m_weapons.cycleWeapon(1); }

    if (input.isKeyJustPressed(GLFW_KEY_LEFT_SHIFT) || input.isKeyJustPressed(GLFW_KEY_RIGHT_SHIFT)) {
        m_super.requestFire();
    }
}

void GameWorld::updateEnemies(float deltaTime) {
    // Update, then drain any children a behaviour queued (Splitter).
    std::vector<std::unique_ptr<EnemyBase>> spawned;
    for (auto& enemy : m_enemies) {
        enemy->update(deltaTime, *this);
        if (enemy->hasPendingSpawns()) {
            for (auto& child : enemy->takePendingSpawns()) {
                if (child) {
                    child->setHandle(m_nextHandle++);
                    spawned.push_back(std::move(child));
                }
            }
        }
    }
    for (auto& child : spawned) {
        m_enemies.push_back(std::move(child));
    }
}

void GameWorld::resolveEnemyDeaths() {
    for (auto& enemy : m_enemies) {
        if (!enemy->hasDeathEvent()) {
            continue;
        }
        const EnemyDeathEvent event = enemy->deathEvent();
        enemy->clearDeathEvent();

        m_score += event.scoreValue;

        // Minions do not roll the full table; they would flood the screen.
        if (!event.isMinion) {
            m_powerups.maybeDropPowerup(event.position, event.archetype, m_difficulty);
        }

        EffectRequest fx;
        fx.kind = event.wasBoss ? EffectKind::BigExplosion : EffectKind::Explosion;
        fx.position = event.position;
        fx.scale = event.wasBoss ? 2.4f : 1.0f;
        spawnEffect(fx);

        if (event.wasBoss) {
            addScreenShake(22.0f, 1.2f);
            m_director.notifyBossDefeated();
            m_secrets.onBossDefeated(m_levelTime);
            pushNotice("BOSS DESTROYED", m_levelDisplayName, NoticeKind::Info);
        }

        m_secrets.onEnemyDestroyed(event.archetype, m_director.currentWaveName(), m_levelTime);
    }

    // Retire dead and off-screen enemies.
    m_enemies.erase(
        std::remove_if(m_enemies.begin(), m_enemies.end(),
                       [](const std::unique_ptr<EnemyBase>& e) {
                           return !e || !e->isAlive() || e->isOffScreen();
                       }),
        m_enemies.end());
}

void GameWorld::updateCollisions(float deltaTime) {
    // --- Player projectiles vs enemies -------------------------------------
    for (Projectile& shot : m_projectiles.playerProjectiles()) {
        if (!shot.alive()) {
            continue;
        }
        for (auto& enemy : m_enemies) {
            if (!enemy->isAlive() || enemy->isInvulnerable()) {
                continue;
            }
            const float reach = shot.radius() + enemy->radius();
            if (distanceSquared(shot.position(), enemy->position()) > reach * reach) {
                continue;
            }

            enemy->takeDamage(shot.damage(), *this);

            EffectRequest fx;
            fx.kind = EffectKind::Impact;
            fx.position = shot.position();
            fx.direction = normalize(shot.velocity());
            spawnEffect(fx);

            // onHit reports whether the projectile is consumed; piercing shots
            // survive and keep travelling.
            if (shot.onHit()) {
                break;
            }
        }
    }

    // --- Enemy projectiles vs player ---------------------------------------
    if (m_player.alive) {
        for (Projectile& shot : m_projectiles.enemyProjectiles()) {
            if (!shot.alive()) {
                continue;
            }
            const float reach = shot.radius() + m_player.radius;
            if (distanceSquared(shot.position(), m_player.position) > reach * reach) {
                continue;
            }
            damagePlayer(shot.damage(), shot.position());
            shot.kill();
        }
    }

    // --- Enemy contact vs player -------------------------------------------
    if (m_player.alive) {
        for (auto& enemy : m_enemies) {
            if (!enemy->isAlive()) {
                continue;
            }
            const float reach = m_player.radius + enemy->radius();
            if (distanceSquared(m_player.position, enemy->position()) > reach * reach) {
                continue;
            }
            // Scaled by dt so contact damage is a rate while overlapping.
            damagePlayer(enemy->contactDamage() * (deltaTime / ContactDamageInterval),
                         enemy->position());
        }
    }
}

void GameWorld::applyBeamDamage(float deltaTime) {
    // The basic laser: a horizontal beam from the ship's nose to the right edge.
    if (m_weapons.laserActive()) {
        const Vector2 origin = m_weapons.laserOrigin();
        const float halfThickness = m_weapons.laserHalfThickness();
        const float damage = m_weapons.laserDamagePerSecond() * deltaTime;
        const float endX = origin.x + m_weapons.laserLength();

        for (auto& enemy : m_enemies) {
            if (!enemy->isAlive() || enemy->isInvulnerable()) {
                continue;
            }
            const Vector2 p = enemy->position();
            if (p.x + enemy->radius() < origin.x || p.x - enemy->radius() > endX) {
                continue;
            }
            if (std::fabs(p.y - origin.y) > halfThickness + enemy->radius()) {
                continue;
            }
            enemy->takeDamage(damage, *this);
        }
    }

    // Superweapon beams: arbitrary angles, so use point-to-segment distance.
    for (const BeamSegment& beam : m_super.beams()) {
        const Vector2 end = add(beam.origin, fromAngle(beam.angle, beam.length));
        const float damage = beam.damagePerSecond * deltaTime;

        for (auto& enemy : m_enemies) {
            if (!enemy->isAlive() || enemy->isInvulnerable()) {
                continue;
            }
            if (distanceToSegment(enemy->position(), beam.origin, end)
                <= beam.halfThickness + enemy->radius()) {
                enemy->takeDamage(damage, *this);
            }
        }
    }
}

void GameWorld::collectPowerups() {
    for (PowerupType type : m_powerups.collectedThisFrame()) {
        switch (type) {
            case PowerupType::CellRepair:
                m_cells.repairLowestBrokenCell();
                pushNotice("CELL REPAIRED", powerupName(type), NoticeKind::Powerup);
                break;
            case PowerupType::EnergyCharge:
                m_cells.addCharge(45.0f);
                pushNotice("ENERGY CHARGE", powerupName(type), NoticeKind::Powerup);
                break;
            default:
                m_weapons.grantWeaponPowerup(type);
                pushNotice(powerupName(type), "Weapon upgraded", NoticeKind::Powerup);
                break;
        }

        m_score += 100;
        m_secrets.onPowerupCollected(type, m_levelTime);

        EffectRequest fx;
        fx.kind = EffectKind::PowerupPickup;
        fx.position = m_player.position;
        spawnEffect(fx);
    }
}

void GameWorld::damagePlayer(float amount, Vector2 source) {
    if (amount <= 0.0f || !m_player.alive || m_player.invulnerable > 0.0f) {
        return;
    }

    m_cells.applyDamage(amount, m_levelTime);
    m_secrets.onPlayerDamaged(amount, m_levelTime);
    m_player.hitFlash = PlayerHitFlashDuration;

    EffectRequest fx;
    fx.kind = EffectKind::Impact;
    fx.position = m_player.position;
    fx.direction = normalize(sub(m_player.position, source));
    spawnEffect(fx);
}

void GameWorld::updateShake(float deltaTime) {
    if (m_shakeTimer <= 0.0f) {
        m_shakeOffset = { 0.0f, 0.0f };
        return;
    }

    m_shakeTimer = std::max(0.0f, m_shakeTimer - deltaTime);
    const float falloff = m_shakeDuration > 0.0f ? (m_shakeTimer / m_shakeDuration) : 0.0f;
    const float magnitude = m_shakeIntensity * falloff;
    m_shakeOffset = { randomRange(-magnitude, magnitude), randomRange(-magnitude, magnitude) };
}

void GameWorld::pushNotice(const std::string& title, const std::string& subtitle, NoticeKind kind) {
    m_notices.push_back(Notice{ title, subtitle, kind });
}

std::vector<Notice> GameWorld::takeNotices() {
    std::vector<Notice> out;
    out.swap(m_notices);
    return out;
}

bool GameWorld::bossStatus(float& healthFraction) const {
    for (const auto& enemy : m_enemies) {
        if (enemy->isAlive() && enemy->isBoss()) {
            healthFraction = enemy->healthFraction();
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------

void GameWorld::buildDrawList(DrawList& out) const {
    out.clear();

    m_starfield.appendDraw(out);
    m_powerups.appendDraw(out);

    for (const auto& enemy : m_enemies) {
        if (enemy->isAlive()) {
            enemy->appendDraw(out);
        }
    }

    // The ship banks with vertical movement, which sells the motion.
    if (m_player.alive) {
        // y is down, so a negative y velocity is movement up the screen.
        SpriteId shipSprite = SpriteId::ShipPlayer;
        if (m_player.velocity.y < -20.0f)      { shipSprite = SpriteId::ShipPlayerBankUp; }
        else if (m_player.velocity.y > 20.0f)  { shipSprite = SpriteId::ShipPlayerBankDown; }

        Color tint = White;
        if (m_player.hitFlash > 0.0f) {
            tint = Color{ 1.0f, 0.5f, 0.5f, 1.0f };
        } else if (m_player.invulnerable > 0.0f) {
            // Blink while the mercy window is active.
            const float blink = 0.55f + 0.45f * std::sin(m_player.invulnerable * 26.0f);
            tint = Color{ 1.0f, 1.0f, 1.0f, blink };
        }

        out.add(shipSprite, m_player.position, Vector2{ 64.0f, 40.0f },
                DrawLayer::Player, tint);
    }

    m_projectiles.appendDraw(out);
    m_weapons.appendDraw(out);
    m_super.appendDraw(out);
    m_particles.appendDraw(out);

    out.sortByLayer();
}

// ---------------------------------------------------------------------------
// IGameWorld
// ---------------------------------------------------------------------------

void GameWorld::spawnPlayerProjectile(const ProjectileSpawn& spawn) {
    m_projectiles.spawn(spawn, Faction::Player);
}

void GameWorld::spawnEnemyProjectile(const ProjectileSpawn& spawn) {
    m_projectiles.spawn(spawn, Faction::Enemy);
}

void GameWorld::spawnPowerup(PowerupType type, Vector2 position) {
    m_powerups.spawn(type, position);
}

void GameWorld::spawnEffect(const EffectRequest& request) {
    playEffect(m_particles, request);
}

Vector2 GameWorld::playerPosition() const {
    return m_player.position;
}

TargetInfo GameWorld::describe(const EnemyBase& enemy) {
    TargetInfo info;
    info.position = enemy.position();
    info.velocity = enemy.velocity();
    info.radius = enemy.radius();
    info.handle = enemy.handle();
    info.isBoss = enemy.isBoss();
    return info;
}

bool GameWorld::findNearestEnemy(Vector2 from, float maxDistance, TargetInfo& out) const {
    const float limitSq = maxDistance > 0.0f ? maxDistance * maxDistance : 0.0f;

    const EnemyBase* best = nullptr;
    float bestSq = 0.0f;
    for (const auto& enemy : m_enemies) {
        if (!enemy->isAlive()) {
            continue;
        }
        // Targets behind the ship are not useful for forward-firing weapons,
        // but homing missiles can turn, so only cull well off the left edge.
        if (enemy->position().x < -50.0f) {
            continue;
        }
        const float d = distanceSquared(from, enemy->position());
        if (limitSq > 0.0f && d > limitSq) {
            continue;
        }
        if (!best || d < bestSq) {
            best = enemy.get();
            bestSq = d;
        }
    }

    if (!best) {
        return false;
    }
    out = describe(*best);
    return true;
}

std::vector<TargetInfo> GameWorld::findEnemies(Vector2 from, float maxDistance, int maxCount) const {
    std::vector<TargetInfo> found;
    if (maxCount <= 0) {
        return found;
    }

    const float limitSq = maxDistance > 0.0f ? maxDistance * maxDistance : 0.0f;

    // Collect candidates with their distances, then take the nearest N.
    std::vector<std::pair<float, const EnemyBase*>> candidates;
    candidates.reserve(m_enemies.size());
    for (const auto& enemy : m_enemies) {
        if (!enemy->isAlive() || enemy->position().x < -50.0f) {
            continue;
        }
        const float d = distanceSquared(from, enemy->position());
        if (limitSq > 0.0f && d > limitSq) {
            continue;
        }
        candidates.emplace_back(d, enemy.get());
    }

    const std::size_t take = std::min(static_cast<std::size_t>(maxCount), candidates.size());
    std::partial_sort(candidates.begin(), candidates.begin() + static_cast<std::ptrdiff_t>(take),
                      candidates.end(),
                      [](const auto& a, const auto& b) { return a.first < b.first; });

    found.reserve(take);
    for (std::size_t i = 0; i < take; ++i) {
        found.push_back(describe(*candidates[i].second));
    }
    return found;
}

bool GameWorld::resolveTarget(std::uint32_t handle, TargetInfo& out) const {
    if (handle == InvalidTarget) {
        return false;
    }
    for (const auto& enemy : m_enemies) {
        if (enemy->handle() == handle && enemy->isAlive()) {
            out = describe(*enemy);
            return true;
        }
    }
    return false;
}

void GameWorld::clearScreen(float bossDamage) {
    int cleared = 0;
    int bossesHit = 0;

    for (auto& enemy : m_enemies) {
        if (!enemy->isAlive()) {
            continue;
        }
        if (enemy->isBoss()) {
            // Bosses survive the bomb but take a heavy hit.
            enemy->takeDamage(bossDamage > 0.0f ? bossDamage : EnergyBombBossDamage, *this);
            ++bossesHit;
        } else {
            enemy->destroy(*this);
            ++cleared;
        }
    }

    EffectRequest fx;
    fx.kind = EffectKind::ScreenClear;
    fx.position = m_player.position;
    fx.scale = 2.0f;
    spawnEffect(fx);

    addScreenShake(24.0f, 0.8f);
    HU_LOG_INFO(LogCat, "Energy Bomb: cleared %d enemies, damaged %d boss(es)", cleared, bossesHit);
}

void GameWorld::addScreenShake(float intensity, float duration) {
    // Keep the strongest active shake rather than letting them stack into
    // something unreadable.
    if (intensity <= m_shakeIntensity && m_shakeTimer > 0.0f) {
        return;
    }
    m_shakeIntensity = intensity;
    m_shakeDuration = std::max(duration, 0.01f);
    m_shakeTimer = m_shakeDuration;
}

} // namespace hu
