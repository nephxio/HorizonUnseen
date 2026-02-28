#pragma once

#include <string>

class GameConfig;

class ConfigManager {
public:
    // Save current config to binary file
    static bool saveConfig(const GameConfig& config, const std::string& filename = "game.dat");
    
    // Load config from binary file
    static bool loadConfig(GameConfig& config, const std::string& filename = "game.dat");
    
    // Get last error message
    static const std::string& getLastError() { return s_lastError; }

private:
    static const uint32_t MAGIC_NUMBER = 0x48555345;  // "HUSE" (Horizon Unseen)
    static const uint32_t CONFIG_VERSION = 1;
    
    static std::string s_lastError;
};
