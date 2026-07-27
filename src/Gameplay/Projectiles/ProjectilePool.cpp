#include "Gameplay/Projectiles/ProjectilePool.h"

#include "Core/DrawList.h"
#include "Core/Log.h"

namespace hu {

ProjectilePool::ProjectilePool() {
    reset();
}

void ProjectilePool::reset() {
    m_player.assign(PlayerCapacity, Projectile{});
    m_enemy.assign(EnemyCapacity, Projectile{});

    rebuildFreeList(m_player, m_playerFree);
    rebuildFreeList(m_enemy, m_enemyFree);

    m_playerExhaustedWarned = false;
    m_enemyExhaustedWarned = false;

    HU_LOG_INFO("Projectile", "Pools reset: %zu player slots, %zu enemy slots",
                PlayerCapacity, EnemyCapacity);
}

void ProjectilePool::rebuildFreeList(const std::vector<Projectile>& storage,
                                     std::vector<std::size_t>& freeList) {
    freeList.clear();
    freeList.reserve(storage.size());
    // Reverse order so pop_back() hands out low indices first, which keeps the
    // active set packed toward the front and makes debug dumps readable.
    for (std::size_t i = storage.size(); i-- > 0;) {
        if (!storage[i].alive()) {
            freeList.push_back(i);
        }
    }
}

Projectile* ProjectilePool::spawnInto(std::vector<Projectile>& storage,
                                      std::vector<std::size_t>& freeList,
                                      bool& warned,
                                      const char* label,
                                      const ProjectileSpawn& request,
                                      Faction faction) {
    if (freeList.empty()) {
        if (!warned) {
            warned = true;
            HU_LOG_WARN("Projectile", "%s projectile pool exhausted (%zu slots); dropping shots",
                        label, storage.size());
        }
        return nullptr;
    }

    const std::size_t index = freeList.back();
    freeList.pop_back();
    Projectile& p = storage[index];
    p.initialize(request, faction);
    return &p;
}

Projectile* ProjectilePool::spawn(const ProjectileSpawn& request, Faction faction) {
    if (faction == Faction::Player) {
        return spawnInto(m_player, m_playerFree, m_playerExhaustedWarned, "Player", request, faction);
    }
    return spawnInto(m_enemy, m_enemyFree, m_enemyExhaustedWarned, "Enemy", request, faction);
}

void ProjectilePool::update(float deltaTime, IGameWorld& world) {
    for (Projectile& p : m_player) {
        if (p.alive()) {
            p.update(deltaTime, world);
        }
    }
    for (Projectile& p : m_enemy) {
        if (p.alive()) {
            p.update(deltaTime, world);
        }
    }

    // Recycling once per frame (rather than per-kill) keeps the free lists
    // consistent no matter who killed a projectile: expiry, culling, or the
    // scene's collision pass calling onHit().
    rebuildFreeList(m_player, m_playerFree);
    rebuildFreeList(m_enemy, m_enemyFree);
}

void ProjectilePool::appendDraw(DrawList& out) const {
    for (const Projectile& p : m_player) {
        p.appendDraw(out);
    }
    for (const Projectile& p : m_enemy) {
        p.appendDraw(out);
    }
}

std::size_t ProjectilePool::activePlayerCount() const {
    return m_player.size() - m_playerFree.size();
}

std::size_t ProjectilePool::activeEnemyCount() const {
    return m_enemy.size() - m_enemyFree.size();
}

} // namespace hu
