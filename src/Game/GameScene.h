#pragma once

#include "Player.h"
#include <memory>
#include <vector>

class InputSystem;

class GameScene {
public:
    GameScene();
    
    void update(float deltaTime, const InputSystem& input);
    
    const Player& getPlayer() const { return m_player; }

private:
    Player m_player;
    
    // TODO: Add these systems:
    // std::vector<std::unique_ptr<Enemy>> m_enemies;
    // std::vector<std::unique_ptr<Bullet>> m_bullets;
    // std::vector<std::unique_ptr<Powerup>> m_powerups;
    // ParticleSystem m_particles;
    // float m_scrollSpeed = 50.0f;
};
