#pragma once

class GameConfig {
public:
    static GameConfig& getInstance() {
        static GameConfig instance;
        return instance;
    }

    // Player Stats
    float playerHitPoints = 100.0f;
    float playerMovementSpeedX = 200.0f;
    float playerMovementSpeedY = 200.0f;
    float playerFireRate = 1.0f;  // Shots per second
    float playerBulletDamage = 10.0f;
    float playerBulletSpeed = 500.0f;

    // Enemy Stats
    float enemyHitPoints = 10.0f;
    float enemyDamage = 50.0f;  // Increased from 20 to make collisions more obvious
    float enemyHorizontalSpeed = 150.0f;
    float enemyDiveSpeed = 300.0f;
    float enemyDiveAngle = 30.0f;

    // Enemy Bullet Stats
    float enemyBulletDamage = 10.0f;
    float enemyBulletSpeed = 200.0f;
    float enemyFireRate = 0.5f;  // Shots per second
    int enemyBulletPoolSize = 100;  // Maximum enemy bullets on screen

    // Spawner Settings
    float spawnerBaseInterval = 2.5f;
    float spawnerIntervalIncrement = 0.5f;

    // Game Settings
    float screenWidth = 1280.0f;
    float screenHeight = 720.0f;

    // Health Cell System
    float healthCellMaxHealth = 100.0f;
    float healthCellMaxEnergy = 100.0f;
    float healthCellEnergyGainRate = 1.0f;
    float healthCellDecayRate = 10.0f;
    float healthCellDamageThresholds[5] = { 25.0f, 25.0f, 25.0f, 25.0f, 25.0f };

private:
    GameConfig() = default;
    ~GameConfig() = default;
    GameConfig(const GameConfig&) = delete;
    GameConfig& operator=(const GameConfig&) = delete;
};
