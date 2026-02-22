#pragma once

struct Vector2 {
    float x, y;
};

class Entity {
public:
    Entity(float x, float y, float health = 100.0f, float damage = 10.0f) 
        : m_position{x, y}
        , m_velocity{0, 0}
        , m_health(health)
        , m_maxHealth(health)
        , m_damage(damage)
        , m_isAlive(true)
    {}

    virtual ~Entity() = default;

    virtual void update(float deltaTime) = 0;

    Vector2 getPosition() const { return m_position; }
    void setPosition(Vector2 pos) { m_position = pos; }

    Vector2 getVelocity() const { return m_velocity; }
    void setVelocity(Vector2 vel) { m_velocity = vel; }

    float getHealth() const { return m_health; }
    void setHealth(float health) { m_health = health; if (m_health <= 0) m_isAlive = false; }

    float getMaxHealth() const { return m_maxHealth; }
    void setMaxHealth(float maxHealth) { m_maxHealth = maxHealth; }

    float getDamage() const { return m_damage; }
    void setDamage(float damage) { m_damage = damage; }

    bool isAlive() const { return m_isAlive; }

    void takeDamage(float damage) {
        m_health -= damage;
        if (m_health <= 0) {
            m_health = 0;
            m_isAlive = false;
        }
    }

protected:
    Vector2 m_position;
    Vector2 m_velocity;
    float m_health;
    float m_maxHealth;
    float m_damage;
    bool m_isAlive;
};
