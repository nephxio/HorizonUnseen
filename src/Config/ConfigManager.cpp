#include "ConfigManager.h"
#include "GameConfig.h"
#include <fstream>
#include <cstring>

std::string ConfigManager::s_lastError = "";

bool ConfigManager::saveConfig(const GameConfig& config, const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    
    if (!file.is_open()) {
        s_lastError = "Failed to open file for writing: " + filename;
        return false;
    }
    
    // Write header
    file.write(reinterpret_cast<const char*>(&MAGIC_NUMBER), sizeof(MAGIC_NUMBER));
    file.write(reinterpret_cast<const char*>(&CONFIG_VERSION), sizeof(CONFIG_VERSION));
    
    // Write screen settings
    file.write(reinterpret_cast<const char*>(&config.screenWidth), sizeof(config.screenWidth));
    file.write(reinterpret_cast<const char*>(&config.screenHeight), sizeof(config.screenHeight));
    
    // Write player stats
    file.write(reinterpret_cast<const char*>(&config.playerHitPoints), sizeof(config.playerHitPoints));
    file.write(reinterpret_cast<const char*>(&config.playerMovementSpeedX), sizeof(config.playerMovementSpeedX));
    file.write(reinterpret_cast<const char*>(&config.playerMovementSpeedY), sizeof(config.playerMovementSpeedY));
    file.write(reinterpret_cast<const char*>(&config.playerFireRate), sizeof(config.playerFireRate));
    file.write(reinterpret_cast<const char*>(&config.playerBulletDamage), sizeof(config.playerBulletDamage));
    file.write(reinterpret_cast<const char*>(&config.playerBulletSpeed), sizeof(config.playerBulletSpeed));
    
    // Write enemy stats
    file.write(reinterpret_cast<const char*>(&config.enemyHitPoints), sizeof(config.enemyHitPoints));
    file.write(reinterpret_cast<const char*>(&config.enemyDamage), sizeof(config.enemyDamage));
    file.write(reinterpret_cast<const char*>(&config.enemyHorizontalSpeed), sizeof(config.enemyHorizontalSpeed));
    file.write(reinterpret_cast<const char*>(&config.enemyDiveSpeed), sizeof(config.enemyDiveSpeed));
    file.write(reinterpret_cast<const char*>(&config.enemyDiveAngle), sizeof(config.enemyDiveAngle));
    file.write(reinterpret_cast<const char*>(&config.enemyBulletDamage), sizeof(config.enemyBulletDamage));
    file.write(reinterpret_cast<const char*>(&config.enemyBulletSpeed), sizeof(config.enemyBulletSpeed));
    file.write(reinterpret_cast<const char*>(&config.enemyFireRate), sizeof(config.enemyFireRate));
    file.write(reinterpret_cast<const char*>(&config.enemyBulletPoolSize), sizeof(config.enemyBulletPoolSize));

    // Write spawner settings
    file.write(reinterpret_cast<const char*>(&config.spawnerBaseInterval), sizeof(config.spawnerBaseInterval));
    file.write(reinterpret_cast<const char*>(&config.spawnerIntervalIncrement), sizeof(config.spawnerIntervalIncrement));
    file.write(reinterpret_cast<const char*>(&config.healthCellMaxHealth), sizeof(config.healthCellMaxHealth));
    file.write(reinterpret_cast<const char*>(&config.healthCellMaxEnergy), sizeof(config.healthCellMaxEnergy));
    file.write(reinterpret_cast<const char*>(&config.healthCellEnergyGainRate), sizeof(config.healthCellEnergyGainRate));
    file.write(reinterpret_cast<const char*>(&config.healthCellDecayRate), sizeof(config.healthCellDecayRate));
    file.write(reinterpret_cast<const char*>(config.healthCellDamageThresholds), sizeof(config.healthCellDamageThresholds));

    // Write health cell settings
    file.write(reinterpret_cast<const char*>(&config.healthCellMaxHealth), sizeof(config.healthCellMaxHealth));
    file.write(reinterpret_cast<const char*>(&config.healthCellMaxEnergy), sizeof(config.healthCellMaxEnergy));
    file.write(reinterpret_cast<const char*>(&config.healthCellEnergyGainRate), sizeof(config.healthCellEnergyGainRate));
    file.write(reinterpret_cast<const char*>(&config.healthCellDecayRate), sizeof(config.healthCellDecayRate));
    file.write(reinterpret_cast<const char*>(config.healthCellDamageThresholds), sizeof(config.healthCellDamageThresholds));

    file.close();
    
    if (!file.good()) {
        s_lastError = "Error occurred during file write";
        return false;
    }
    
    s_lastError = "";
    return true;
}

bool ConfigManager::loadConfig(GameConfig& config, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    
    if (!file.is_open()) {
        s_lastError = "Config file not found: " + filename;
        return false;
    }
    
    // Read and validate header
    uint32_t magic, version;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    
    if (magic != MAGIC_NUMBER) {
        s_lastError = "Invalid config file format (magic number mismatch)";
        file.close();
        return false;
    }
    
    if (version != CONFIG_VERSION) {
        s_lastError = "Config file version mismatch (expected: " + 
                      std::to_string(CONFIG_VERSION) + ", got: " + std::to_string(version) + ")";
        file.close();
        return false;
    }
    
    // Read screen settings
    file.read(reinterpret_cast<char*>(&config.screenWidth), sizeof(config.screenWidth));
    file.read(reinterpret_cast<char*>(&config.screenHeight), sizeof(config.screenHeight));
    
    // Read player stats
    file.read(reinterpret_cast<char*>(&config.playerHitPoints), sizeof(config.playerHitPoints));
    file.read(reinterpret_cast<char*>(&config.playerMovementSpeedX), sizeof(config.playerMovementSpeedX));
    file.read(reinterpret_cast<char*>(&config.playerMovementSpeedY), sizeof(config.playerMovementSpeedY));
    file.read(reinterpret_cast<char*>(&config.playerFireRate), sizeof(config.playerFireRate));
    file.read(reinterpret_cast<char*>(&config.playerBulletDamage), sizeof(config.playerBulletDamage));
    file.read(reinterpret_cast<char*>(&config.playerBulletSpeed), sizeof(config.playerBulletSpeed));
    
    // Read enemy stats
    file.read(reinterpret_cast<char*>(&config.enemyHitPoints), sizeof(config.enemyHitPoints));
    file.read(reinterpret_cast<char*>(&config.enemyDamage), sizeof(config.enemyDamage));
    file.read(reinterpret_cast<char*>(&config.enemyHorizontalSpeed), sizeof(config.enemyHorizontalSpeed));
    file.read(reinterpret_cast<char*>(&config.enemyDiveSpeed), sizeof(config.enemyDiveSpeed));
    file.read(reinterpret_cast<char*>(&config.enemyDiveAngle), sizeof(config.enemyDiveAngle));
    file.read(reinterpret_cast<char*>(&config.enemyBulletDamage), sizeof(config.enemyBulletDamage));
    file.read(reinterpret_cast<char*>(&config.enemyBulletSpeed), sizeof(config.enemyBulletSpeed));
    file.read(reinterpret_cast<char*>(&config.enemyFireRate), sizeof(config.enemyFireRate));
    file.read(reinterpret_cast<char*>(&config.enemyBulletPoolSize), sizeof(config.enemyBulletPoolSize));

    // Read spawner settings
    file.read(reinterpret_cast<char*>(&config.spawnerBaseInterval), sizeof(config.spawnerBaseInterval));
    file.read(reinterpret_cast<char*>(&config.spawnerIntervalIncrement), sizeof(config.spawnerIntervalIncrement));
    file.read(reinterpret_cast<char*>(&config.healthCellMaxHealth), sizeof(config.healthCellMaxHealth));
    file.read(reinterpret_cast<char*>(&config.healthCellMaxEnergy), sizeof(config.healthCellMaxEnergy));
    file.read(reinterpret_cast<char*>(&config.healthCellEnergyGainRate), sizeof(config.healthCellEnergyGainRate));
    file.read(reinterpret_cast<char*>(&config.healthCellDecayRate), sizeof(config.healthCellDecayRate));
    file.read(reinterpret_cast<char*>(config.healthCellDamageThresholds), sizeof(config.healthCellDamageThresholds));

    // Read health cell settings
    file.read(reinterpret_cast<char*>(&config.healthCellMaxHealth), sizeof(config.healthCellMaxHealth));
    file.read(reinterpret_cast<char*>(&config.healthCellMaxEnergy), sizeof(config.healthCellMaxEnergy));
    file.read(reinterpret_cast<char*>(&config.healthCellEnergyGainRate), sizeof(config.healthCellEnergyGainRate));
    file.read(reinterpret_cast<char*>(&config.healthCellDecayRate), sizeof(config.healthCellDecayRate));
    file.read(reinterpret_cast<char*>(config.healthCellDamageThresholds), sizeof(config.healthCellDamageThresholds));

    file.close();
    
    if (!file.good()) {
        s_lastError = "Error occurred during file read";
        return false;
    }
    
    s_lastError = "";
    return true;
}
