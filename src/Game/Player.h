#pragma once

#include "Entity.h"

class InputSystem;

class Player : public Entity {
public:
    Player();

    void update(float deltaTime) override;
    void handleInput(const InputSystem& input, float deltaTime);

    float getHealth() const { return m_health; }
    void setHealth(float health) { m_health = health; }

private:
    float m_health;
};
