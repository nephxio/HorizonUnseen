#pragma once

#include "Config/GameConfig.h"
#include <array>
#include <algorithm>
#include <cstddef>

struct HealthCell {
    float currentHealth = 100.0f;
    float maxHealth = 100.0f;
    float currentEnergy = 0.0f;
    float maxEnergy = 100.0f;
    float damageThreshold = 20.0f;
    bool broken = false;
};

class HealthSystem {
public:
    static constexpr std::size_t CellCount = 5;

    HealthSystem() {
        syncFromConfig();
    }

    void syncFromConfig() {
        auto& config = GameConfig::getInstance();
        for (std::size_t i = 0; i < CellCount; ++i) {
            m_cells[i].maxHealth = config.healthCellMaxHealth;
            m_cells[i].maxEnergy = config.healthCellMaxEnergy;
            m_cells[i].damageThreshold = config.healthCellDamageThresholds[i];
            if (m_cells[i].currentHealth > m_cells[i].maxHealth) {
                m_cells[i].currentHealth = m_cells[i].maxHealth;
            }
            if (m_cells[i].currentEnergy > m_cells[i].maxEnergy) {
                m_cells[i].currentEnergy = m_cells[i].maxEnergy;
            }
        }
    }

    void queueDamage(float damage) {
        if (damage > 0.0f) {
            m_pendingDamage += damage;
        }
    }

    void update(float deltaTime) {
        syncFromConfig();

        if (m_pendingDamage <= 0.0f) {
            return;
        }

        float safeDelta = std::max(deltaTime, 0.0001f);
        float incomingRate = m_pendingDamage / safeDelta;

        int activeCell = getActiveCellIndex();
        if (activeCell < 0) {
            m_pendingDamage = 0.0f;
            return;
        }

        auto& cell = m_cells[static_cast<std::size_t>(activeCell)];
        auto& config = GameConfig::getInstance();

        if (incomingRate <= cell.damageThreshold) {
            cell.currentEnergy = std::min(cell.maxEnergy, cell.currentEnergy + m_pendingDamage * config.healthCellEnergyGainRate);
        } else {
            float excessRate = incomingRate - cell.damageThreshold;
            float hpLoss = excessRate * config.healthCellDecayRate * deltaTime;
            cell.currentHealth = std::max(0.0f, cell.currentHealth - hpLoss);
            if (cell.currentHealth <= 0.0f) {
                cell.currentHealth = 0.0f;
                cell.broken = true;
            }
        }

        m_pendingDamage = 0.0f;
    }

    void healCell(std::size_t index, float amount) {
        if (index >= CellCount || amount <= 0.0f) {
            return;
        }

        auto& cell = m_cells[index];
        cell.currentHealth = std::min(cell.maxHealth, cell.currentHealth + amount);
        if (cell.currentHealth > 0.0f) {
            cell.broken = false;
        }
    }

    void restoreEnergy(std::size_t index, float amount) {
        if (index >= CellCount || amount <= 0.0f) {
            return;
        }

        auto& cell = m_cells[index];
        cell.currentEnergy = std::min(cell.maxEnergy, cell.currentEnergy + amount);
    }

    bool isAlive() const {
        return getAliveCellCount() > 0;
    }

    std::size_t getAliveCellCount() const {
        std::size_t count = 0;
        for (const auto& cell : m_cells) {
            if (!cell.broken) {
                ++count;
            }
        }
        return count;
    }

    float getTotalHealth() const {
        float total = 0.0f;
        for (const auto& cell : m_cells) {
            total += cell.currentHealth;
        }
        return total;
    }

    float getTotalMaxHealth() const {
        float total = 0.0f;
        for (const auto& cell : m_cells) {
            total += cell.maxHealth;
        }
        return total;
    }

    float getTotalEnergy() const {
        float total = 0.0f;
        for (const auto& cell : m_cells) {
            total += cell.currentEnergy;
        }
        return total;
    }

    float getTotalMaxEnergy() const {
        float total = 0.0f;
        for (const auto& cell : m_cells) {
            total += cell.maxEnergy;
        }
        return total;
    }

    const HealthCell& getCell(std::size_t index) const {
        return m_cells[index];
    }

    std::array<HealthCell, CellCount>& getCells() { return m_cells; }
    const std::array<HealthCell, CellCount>& getCells() const { return m_cells; }

private:
    int getActiveCellIndex() const {
        for (std::size_t i = 0; i < CellCount; ++i) {
            if (!m_cells[i].broken) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    std::array<HealthCell, CellCount> m_cells{};
    float m_pendingDamage = 0.0f;
};
