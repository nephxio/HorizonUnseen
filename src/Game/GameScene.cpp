#include "GameScene.h"
#include "Systems/InputSystem.h"

GameScene::GameScene() : m_player() {}

void GameScene::update(float deltaTime, const InputSystem& input) {
    // Update player
    m_player.handleInput(input, deltaTime);
    m_player.update(deltaTime);
    
    // TODO: Update enemies
    // TODO: Update bullets
    // TODO: Handle collisions
    // TODO: Spawn enemy waves
    // TODO: Update particles
    // TODO: Handle background scrolling
}
