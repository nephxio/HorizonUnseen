#pragma once

// Fixed-capacity projectile storage.
//
// Two independent pools -- player and enemy -- so a bullet-hell enemy pattern
// can never starve the player's own weapons. Slots are pre-allocated at reset()
// and recycled through a free list, so firing never allocates.

#include "Core/GameTypes.h"
#include "Gameplay/IGameWorld.h"
#include "Gameplay/Projectiles/Projectile.h"

#include <cstddef>
#include <vector>

namespace hu {

class DrawList;

class ProjectilePool {
public:
    static constexpr std::size_t PlayerCapacity = 2048;
    static constexpr std::size_t EnemyCapacity = 4096;

    ProjectilePool();

    // Clears both pools and rebuilds the free lists. Also re-arms the
    // "pool exhausted" warnings so a new level reports its own problems.
    void reset();

    // Returns the projectile that was started, or nullptr when the pool is
    // full. The pointer is stable until the next reset().
    Projectile* spawn(const ProjectileSpawn& request, Faction faction);

    void update(float deltaTime, IGameWorld& world);
    void appendDraw(DrawList& out) const;

    // Iteration for collision resolution. Entries with alive() == false are
    // dormant slots and must be skipped by the caller.
    std::vector<Projectile>& playerProjectiles() { return m_player; }
    const std::vector<Projectile>& playerProjectiles() const { return m_player; }
    std::vector<Projectile>& enemyProjectiles() { return m_enemy; }
    const std::vector<Projectile>& enemyProjectiles() const { return m_enemy; }

    std::size_t activePlayerCount() const;
    std::size_t activeEnemyCount() const;

private:
    Projectile* spawnInto(std::vector<Projectile>& storage,
                          std::vector<std::size_t>& freeList,
                          bool& warned,
                          const char* label,
                          const ProjectileSpawn& request,
                          Faction faction);

    // Rebuilds `freeList` from every dormant slot in `storage`.
    static void rebuildFreeList(const std::vector<Projectile>& storage,
                                std::vector<std::size_t>& freeList);

    std::vector<Projectile> m_player;
    std::vector<Projectile> m_enemy;
    std::vector<std::size_t> m_playerFree;
    std::vector<std::size_t> m_enemyFree;
    bool m_playerExhaustedWarned = false;
    bool m_enemyExhaustedWarned = false;
};

} // namespace hu
