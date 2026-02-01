#pragma once

#include "Entity.h"

class InputSystem;

class Player : public Entity {
public:
    Player();
    
    void update(float deltaTime) override;
    void handleInput(const InputSystem& input, float deltaTime);

private:
    float m_speed = 200.0f;
    float m_health = 100.0f;
};
