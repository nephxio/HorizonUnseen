#include "Sim/SimApi.h"

#include "Core/Log.h"
#include "Core/Math.h"
#include "Gameplay/GameWorld.h"
#include "Gameplay/PlayerCommand.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

using namespace hu;

// ---------------------------------------------------------------------------
// Observation layout
//
// Everything is scaled into roughly [-1, 1]. Positions are relative to the
// ship, not absolute: what matters to a dodging policy is where a bullet is
// with respect to the player, and relative coordinates make the learned
// behaviour independent of where on the field it happens.
// ---------------------------------------------------------------------------

constexpr int kTrackedBullets = 24;   // Nearest enemy projectiles.
constexpr int kTrackedEnemies = 8;

constexpr int kPlayerFields = 4;      // position (2), velocity (2)
constexpr int kCellFields = 15;       // 5 cells x (health, charge, broken)
constexpr int kWeaponFields = 9;      // one-hot (4), levels (4), charged cells (1)
constexpr int kBulletFields = kTrackedBullets * 4;   // dx, dy, vx, vy
constexpr int kEnemyFields = kTrackedEnemies * 3;    // dx, dy, health
constexpr int kLevelFields = 3;       // progress, boss active, boss health

constexpr int kObservationSize =
    kPlayerFields + kCellFields + kWeaponFields + kBulletFields + kEnemyFields + kLevelFields;

// Action = one of 9 movement directions, crossed with firing the superweapon
// or not. The main gun is always held: there is no situation in which not
// shooting scores better, so spending policy capacity on it would be waste.
constexpr int kMoveOptions = 9;
constexpr int kActionCount = kMoveOptions * 2;

constexpr int kInfoSize = 10;

// Normalisers. Distances are divided by the screen diagonal-ish scale so a
// bullet across the map lands near 1.
constexpr float kPositionScale = 720.0f;
constexpr float kVelocityScale = 600.0f;

// The run is capped so a passive policy that survives by hiding cannot stall
// training forever.
constexpr int kMaxStepsPerEpisode = 20000;

// --- Reward shaping --------------------------------------------------------
//
// Score is the objective, so it dominates. The rest are small nudges that make
// the signal less sparse: without them the agent gets no feedback at all until
// it happens to shoot something.
constexpr float kScoreRewardScale = 0.01f;
constexpr float kSurvivalRewardPerStep = 0.01f;
constexpr float kDeathPenalty = -5.0f;
constexpr float kCellBreakPenalty = -1.5f;
constexpr float kLevelCompleteBonus = 25.0f;

struct MoveVector {
    float x, y;
};

// Index 0 is "hold still"; the rest are the eight compass directions.
constexpr MoveVector kMoves[kMoveOptions] = {
    {  0.0f,  0.0f },
    {  0.0f, -1.0f },   // up (y is down in world space)
    {  0.0f,  1.0f },   // down
    { -1.0f,  0.0f },
    {  1.0f,  0.0f },
    { -0.7071f, -0.7071f },
    {  0.7071f, -0.7071f },
    { -0.7071f,  0.7071f },
    {  0.7071f,  0.7071f }
};

struct SimEnv {
    GameWorld world;
    int difficulty = 0;
    unsigned int seed = 0;

    long long lastScore = 0;
    std::size_t lastBrokenCells = 0;
    int steps = 0;
    bool done = false;

    // Superweapon input is edge-triggered, so a policy holding the "fire super"
    // action must not re-trigger every frame of the skip window.
    bool superLatched = false;
};

// Writes up to `count` entries of `stride` floats, zero-filling the remainder
// so the observation is always the same shape.
void padRemaining(float* out, int& cursor, int written, int count, int stride) {
    for (int i = written; i < count; ++i) {
        for (int f = 0; f < stride; ++f) {
            out[cursor++] = 0.0f;
        }
    }
}

void writeObservation(const SimEnv& env, float* out) {
    const GameWorld& world = env.world;
    const PlayerShip& player = world.player();

    int cursor = 0;

    // --- Player -----------------------------------------------------------
    out[cursor++] = player.position.x / world.screenWidth() * 2.0f - 1.0f;
    out[cursor++] = player.position.y / world.screenHeight() * 2.0f - 1.0f;
    out[cursor++] = clampf(player.velocity.x / kVelocityScale, -1.0f, 1.0f);
    out[cursor++] = clampf(player.velocity.y / kVelocityScale, -1.0f, 1.0f);

    // --- Energy cells -----------------------------------------------------
    const EnergyCellSystem& cells = world.cells();
    for (std::size_t i = 0; i < EnergyCellSystem::CellCount; ++i) {
        const EnergyCell& cell = cells.cell(i);
        out[cursor++] = cell.maxHealth > 0.0f ? cell.health / cell.maxHealth : 0.0f;
        out[cursor++] = cell.maxCharge > 0.0f ? cell.charge / cell.maxCharge : 0.0f;
        out[cursor++] = cell.broken ? 1.0f : 0.0f;
    }

    // --- Weapons ----------------------------------------------------------
    const WeaponSystem& weapons = world.weapons();
    for (std::size_t i = 0; i < WeaponTypeCount; ++i) {
        out[cursor++] = (weapons.current() == static_cast<WeaponType>(i)) ? 1.0f : 0.0f;
    }
    for (std::size_t i = 0; i < WeaponTypeCount; ++i) {
        out[cursor++] = static_cast<float>(weapons.level(static_cast<WeaponType>(i))) /
                        static_cast<float>(MaxWeaponLevel);
    }
    out[cursor++] = static_cast<float>(cells.chargedCellCount()) /
                    static_cast<float>(EnergyCellSystem::CellCount);

    // --- Nearest bullets --------------------------------------------------
    //
    // Threat is ranked by distance. A more sophisticated ranking (time to
    // impact) would be better, but distance is a strong enough proxy and is
    // cheap to compute every step.
    struct Ranked {
        float distanceSq;
        Vector2 relative;
        Vector2 velocity;
    };
    static std::vector<Ranked> bullets;   // Reused: stepping must not allocate.
    bullets.clear();

    for (const Projectile& shot : world.projectiles().enemyProjectiles()) {
        if (!shot.alive()) {
            continue;
        }
        const Vector2 rel = sub(shot.position(), player.position);
        bullets.push_back(Ranked{ lengthSquared(rel), rel, shot.velocity() });
    }

    const std::size_t bulletCount = std::min(bullets.size(),
                                             static_cast<std::size_t>(kTrackedBullets));
    std::partial_sort(bullets.begin(),
                      bullets.begin() + static_cast<std::ptrdiff_t>(bulletCount),
                      bullets.end(),
                      [](const Ranked& a, const Ranked& b) { return a.distanceSq < b.distanceSq; });

    for (std::size_t i = 0; i < bulletCount; ++i) {
        out[cursor++] = clampf(bullets[i].relative.x / kPositionScale, -1.0f, 1.0f);
        out[cursor++] = clampf(bullets[i].relative.y / kPositionScale, -1.0f, 1.0f);
        out[cursor++] = clampf(bullets[i].velocity.x / kVelocityScale, -1.0f, 1.0f);
        out[cursor++] = clampf(bullets[i].velocity.y / kVelocityScale, -1.0f, 1.0f);
    }
    padRemaining(out, cursor, static_cast<int>(bulletCount), kTrackedBullets, 4);

    // --- Nearest enemies --------------------------------------------------
    struct RankedEnemy {
        float distanceSq;
        Vector2 relative;
        float health;
    };
    static std::vector<RankedEnemy> enemies;
    enemies.clear();

    // findEnemies already returns the nearest N sorted by distance.
    for (const TargetInfo& info : world.findEnemies(player.position, 0.0f, kTrackedEnemies)) {
        const Vector2 rel = sub(info.position, player.position);
        enemies.push_back(RankedEnemy{ lengthSquared(rel), rel, info.isBoss ? 1.0f : 0.5f });
    }

    const std::size_t enemyCount = std::min(enemies.size(),
                                            static_cast<std::size_t>(kTrackedEnemies));
    for (std::size_t i = 0; i < enemyCount; ++i) {
        out[cursor++] = clampf(enemies[i].relative.x / kPositionScale, -1.0f, 1.0f);
        out[cursor++] = clampf(enemies[i].relative.y / kPositionScale, -1.0f, 1.0f);
        out[cursor++] = enemies[i].health;
    }
    padRemaining(out, cursor, static_cast<int>(enemyCount), kTrackedEnemies, 3);

    // --- Level ------------------------------------------------------------
    out[cursor++] = world.director().progress01();
    float bossHealth = 0.0f;
    const bool bossActive = world.bossStatus(bossHealth);
    out[cursor++] = bossActive ? 1.0f : 0.0f;
    out[cursor++] = bossHealth;
}

} // namespace

// ---------------------------------------------------------------------------
// Exported API
// ---------------------------------------------------------------------------

HU_SIM_API int huSimObservationSize() { return kObservationSize; }
HU_SIM_API int huSimActionCount() { return kActionCount; }
HU_SIM_API int huSimInfoSize() { return kInfoSize; }

HU_SIM_API HuSimHandle huSimCreate(int difficulty, unsigned int seed) {
    // Training runs are long and would otherwise write a multi-gigabyte log.
    hu::Log::setLevel(hu::LogLevel::Error);

    SimEnv* env = new SimEnv();
    env->difficulty = difficulty;
    env->seed = seed;
    return env;
}

HU_SIM_API void huSimDestroy(HuSimHandle handle) {
    delete static_cast<SimEnv*>(handle);
}

HU_SIM_API void huSimReset(HuSimHandle handle, float* observation) {
    SimEnv* env = static_cast<SimEnv*>(handle);
    if (!env) {
        return;
    }

    // Effects and scatter use the global RNG; seeding it makes an episode
    // reproducible given the same action sequence.
    std::srand(env->seed);

    env->world.startLevel("test_level",
                          env->difficulty == 1 ? DifficultyMode::BulletHell
                                               : DifficultyMode::Normal);
    env->lastScore = 0;
    env->lastBrokenCells = 0;
    env->steps = 0;
    env->done = false;
    env->superLatched = false;

    if (observation) {
        writeObservation(*env, observation);
    }
}

HU_SIM_API float huSimStep(HuSimHandle handle,
                           int action,
                           int frameSkip,
                           float* observation,
                           int* done) {
    SimEnv* env = static_cast<SimEnv*>(handle);
    if (!env) {
        return 0.0f;
    }

    if (env->done) {
        if (observation) {
            writeObservation(*env, observation);
        }
        if (done) {
            *done = 1;
        }
        return 0.0f;
    }

    action = std::clamp(action, 0, kActionCount - 1);
    const int moveIndex = action % kMoveOptions;
    const bool wantSuper = (action / kMoveOptions) != 0;

    PlayerCommand command;
    command.moveX = kMoves[moveIndex].x;
    command.moveY = kMoves[moveIndex].y;
    command.fire = true;

    // Edge-trigger the superweapon across the skip window: hold the action and
    // it fires once, not once per tick.
    if (wantSuper && !env->superLatched) {
        command.fireSuperweapon = true;
        env->superLatched = true;
    } else if (!wantSuper) {
        env->superLatched = false;
    }

    const float dt = 1.0f / 60.0f;
    const int ticks = std::clamp(frameSkip, 1, 16);

    for (int i = 0; i < ticks; ++i) {
        env->world.update(dt, command);
        // Only the first tick of the window may fire the superweapon.
        command.fireSuperweapon = false;

        // Notices accumulate unboundedly if nothing drains them.
        env->world.takeNotices();

        if (env->world.playerDead() || env->world.levelComplete()) {
            break;
        }
    }

    ++env->steps;

    // --- Reward -----------------------------------------------------------
    float reward = 0.0f;

    const long long score = env->world.score();
    reward += static_cast<float>(score - env->lastScore) * kScoreRewardScale;
    env->lastScore = score;

    reward += kSurvivalRewardPerStep;

    const std::size_t broken = env->world.cells().brokenCellCount();
    if (broken > env->lastBrokenCells) {
        reward += kCellBreakPenalty * static_cast<float>(broken - env->lastBrokenCells);
    }
    env->lastBrokenCells = broken;

    if (env->world.playerDead()) {
        reward += kDeathPenalty;
        env->done = true;
    } else if (env->world.levelComplete()) {
        reward += kLevelCompleteBonus;
        env->done = true;
    } else if (env->steps >= kMaxStepsPerEpisode) {
        env->done = true;
    }

    if (observation) {
        writeObservation(*env, observation);
    }
    if (done) {
        *done = env->done ? 1 : 0;
    }

    return reward;
}

HU_SIM_API void huSimInfo(HuSimHandle handle, float* info) {
    SimEnv* env = static_cast<SimEnv*>(handle);
    if (!env || !info) {
        return;
    }

    const GameWorld& world = env->world;
    float bossHealth = 0.0f;
    const bool bossActive = world.bossStatus(bossHealth);

    info[0] = static_cast<float>(world.score());
    info[1] = world.director().elapsedTime();
    info[2] = static_cast<float>(world.cells().brokenCellCount());
    info[3] = static_cast<float>(world.cells().chargedCellCount());
    info[4] = static_cast<float>(world.grazeCount());
    info[5] = world.director().progress01();
    info[6] = static_cast<float>(world.enemyCount());
    info[7] = static_cast<float>(world.projectiles().activeEnemyCount());
    info[8] = bossActive ? bossHealth : 0.0f;
    info[9] = static_cast<float>(env->steps);
}
