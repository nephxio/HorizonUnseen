#pragma once

struct Vector2 {
    float x, y;
};

class Entity {
public:
    Entity(float x, float y) : m_position{x, y}, m_velocity{0, 0} {}
    virtual ~Entity() = default;

    virtual void update(float deltaTime) = 0;

    Vector2 getPosition() const { return m_position; }
    void setPosition(Vector2 pos) { m_position = pos; }
    
    Vector2 getVelocity() const { return m_velocity; }
    void setVelocity(Vector2 vel) { m_velocity = vel; }

protected:
    Vector2 m_position;
    Vector2 m_velocity;
};
