#pragma once

#include "Entity.h"

enum class EnemyState {
    MovingLeft,
    Diving
};

class Enemy : public Entity {
public:
    Enemy(float x, float y);

    void update(float deltaTime) override;

    bool isOffScreen() const;
    EnemyState getState() const { return m_state; }

private:
    EnemyState m_state;
    float m_diveThreshold;
};
