#include "Gameplay/Power/EnergyCellSystem.h"

#include "Config/GameConfig.h"
#include "Core/Log.h"
#include "Core/Math.h"

namespace hu {

// ---------------------------------------------------------------------------
// Tuning constants (this file's single tunable block).
//
// GameConfig supplies the base scale (healthCellMaxHealth, healthCellMaxEnergy,
// healthCellDamageThresholds); the multipliers below turn that flat baseline
// into the cell-1-tanky / cell-5-fragile gradient the design calls for.
// ---------------------------------------------------------------------------
namespace {

// Rolling window used to decide absorb-vs-break.
constexpr float DamageWindowSeconds = 5.0f;

// Health multipliers applied to GameConfig::healthCellMaxHealth.
// Cell 1 is the armoured battery, cell 5 the fragile capacitor.
constexpr float FirstCellHealthScale = 2.0f;   // cell 1 -> 200 hp at default cfg
constexpr float LastCellHealthScale = 0.6f;    // cell 5 ->  60 hp

// Charge multipliers applied to GameConfig::healthCellMaxEnergy. Inverted
// relative to health: the flimsy cells hold the most energy.
constexpr float FirstCellChargeScale = 0.5f;   // cell 1 ->  50 charge
constexpr float LastCellChargeScale = 1.5f;    // cell 5 -> 150 charge

// Damage-rate threshold multipliers applied to
// GameConfig::healthCellDamageThresholds[i]. Later cells tolerate a slightly
// higher sustained rate before the ship gives up absorbing.
constexpr float FirstCellThresholdScale = 1.0f;
constexpr float LastCellThresholdScale = 1.6f;

// Ignore absurd/degenerate damage values rather than propagating NaN.
constexpr float MinimumDamage = 1e-4f;

float interpolateAcross(std::size_t index, std::size_t count, float first, float last) {
    if (count <= 1) {
        return first;
    }
    const float t = static_cast<float>(index) / static_cast<float>(count - 1);
    return lerp(first, last, t);
}

} // namespace

// ---------------------------------------------------------------------------
// EnergyCell
// ---------------------------------------------------------------------------

bool EnergyCell::isCharged() const {
    return !broken && maxCharge > 0.0f && charge >= maxCharge - 1e-3f;
}

// ---------------------------------------------------------------------------
// EnergyCellSystem
// ---------------------------------------------------------------------------

EnergyCellSystem::EnergyCellSystem() {
    reset();
}

void EnergyCellSystem::configureCells() {
    const GameConfig& cfg = GameConfig::getInstance();

    for (std::size_t i = 0; i < CellCount; ++i) {
        EnergyCell& c = m_cells[i];
        c.maxHealth = cfg.healthCellMaxHealth *
                      interpolateAcross(i, CellCount, FirstCellHealthScale, LastCellHealthScale);
        c.maxCharge = cfg.healthCellMaxEnergy *
                      interpolateAcross(i, CellCount, FirstCellChargeScale, LastCellChargeScale);
        c.damageRateThreshold = cfg.healthCellDamageThresholds[i] *
                                interpolateAcross(i, CellCount, FirstCellThresholdScale, LastCellThresholdScale);
        c.health = c.maxHealth;
        c.charge = 0.0f;
        c.broken = false;
    }
}

void EnergyCellSystem::reset() {
    configureCells();
    m_window.clear();
    m_windowTotal = 0.0f;
    m_lastTime = 0.0f;
    m_deathLogged = false;

    HU_LOG_INFO("Cells", "Reset: %zu cells online, total hp %.1f, window %.1fs",
                CellCount, static_cast<double>(totalMaxHealth()),
                static_cast<double>(DamageWindowSeconds));
    for (std::size_t i = 0; i < CellCount; ++i) {
        HU_LOG_DEBUG("Cells", "  cell %zu: hp %.1f, charge cap %.1f, threshold %.2f dps",
                     i + 1, static_cast<double>(m_cells[i].maxHealth),
                     static_cast<double>(m_cells[i].maxCharge),
                     static_cast<double>(m_cells[i].damageRateThreshold));
    }
}

void EnergyCellSystem::pruneWindow(float currentTime) {
    const float cutoff = currentTime - DamageWindowSeconds;
    while (!m_window.empty() && m_window.front().time < cutoff) {
        m_windowTotal -= m_window.front().amount;
        m_window.pop_front();
    }
    if (m_window.empty()) {
        // Guard against float drift accumulating in the running total.
        m_windowTotal = 0.0f;
    }
}

void EnergyCellSystem::update(float /*deltaTime*/, float currentTime) {
    m_lastTime = currentTime;
    pruneWindow(currentTime);
}

float EnergyCellSystem::windowedDamageRate() const {
    return m_windowTotal / DamageWindowSeconds;
}

float EnergyCellSystem::activeThreshold() const {
    // The cell that would receive charge decides how much punishment the ship
    // can shrug off right now.
    for (std::size_t i = 0; i < CellCount; ++i) {
        const EnergyCell& c = m_cells[i];
        if (!c.broken && c.charge < c.maxCharge) {
            return c.damageRateThreshold;
        }
    }
    // Everything charged: fall back to the highest unbroken cell.
    for (std::size_t i = CellCount; i-- > 0;) {
        if (!m_cells[i].broken) {
            return m_cells[i].damageRateThreshold;
        }
    }
    return 0.0f;
}

void EnergyCellSystem::routeToCharge(float amount) {
    float remaining = amount;
    for (std::size_t i = 0; i < CellCount && remaining > 0.0f; ++i) {
        EnergyCell& c = m_cells[i];
        if (c.broken || c.charge >= c.maxCharge) {
            continue;   // Broken cells hold no charge; full ones overflow up.
        }
        const bool wasCharged = c.isCharged();
        const float space = c.maxCharge - c.charge;
        const float taken = remaining < space ? remaining : space;
        c.charge += taken;
        remaining -= taken;
        if (!wasCharged && c.isCharged()) {
            HU_LOG_INFO("Cells", "Cell %zu CHARGED (%.1f/%.1f) -- %d cells now ready",
                        i + 1, static_cast<double>(c.charge),
                        static_cast<double>(c.maxCharge), chargedCellCount());
        }
    }
}

void EnergyCellSystem::routeToHealth(float amount) {
    float remaining = amount;
    for (std::size_t i = CellCount; i-- > 0 && remaining > 0.0f;) {
        EnergyCell& c = m_cells[i];
        if (c.broken) {
            continue;
        }
        const float taken = remaining < c.health ? remaining : c.health;
        c.health -= taken;
        remaining -= taken;
        if (c.health <= 0.0f) {
            c.health = 0.0f;
            c.charge = 0.0f;   // A broken cell cannot hold charge.
            c.broken = true;
            HU_LOG_INFO("Cells", "Cell %zu BROKEN (%zu/%zu cells down)",
                        i + 1, brokenCellCount(), CellCount);
        }
    }

    if (!isAlive() && !m_deathLogged) {
        m_deathLogged = true;
        HU_LOG_INFO("Cells", "PLAYER DEAD: all %zu cells broken (windowed rate %.2f dps)",
                    CellCount, static_cast<double>(windowedDamageRate()));
    }
}

void EnergyCellSystem::applyDamage(float amount, float currentTime) {
    if (amount <= MinimumDamage || !isAlive()) {
        return;
    }

    m_lastTime = currentTime;

    // The incoming hit counts toward the window it is judged against, so a
    // single enormous hit can overwhelm the ship on its own.
    pruneWindow(currentTime);
    m_window.push_back(DamageEvent{ currentTime, amount });
    m_windowTotal += amount;

    const float rate = windowedDamageRate();
    const float threshold = activeThreshold();

    if (rate < threshold) {
        HU_LOG_DEBUG("Cells", "Absorb %.1f dmg -> charge (rate %.2f < threshold %.2f dps)",
                     static_cast<double>(amount), static_cast<double>(rate),
                     static_cast<double>(threshold));
        routeToCharge(amount);
    } else {
        HU_LOG_DEBUG("Cells", "Overwhelmed by %.1f dmg -> hp (rate %.2f >= threshold %.2f dps)",
                     static_cast<double>(amount), static_cast<double>(rate),
                     static_cast<double>(threshold));
        routeToHealth(amount);
    }
}

int EnergyCellSystem::chargedCellCount() const {
    int count = 0;
    for (std::size_t i = 0; i < CellCount; ++i) {
        if (m_cells[i].isCharged()) {
            ++count;
        }
    }
    return count;
}

bool EnergyCellSystem::consumeCharge(int cellCount) {
    if (cellCount <= 0) {
        return false;
    }
    if (chargedCellCount() < cellCount) {
        HU_LOG_WARN("Cells", "consumeCharge(%d) refused: only %d cells charged",
                    cellCount, chargedCellCount());
        return false;
    }

    int remaining = cellCount;
    // Drain from the top charged cell downward so the low, quick-to-refill
    // cells are the ones left holding energy.
    for (std::size_t i = CellCount; i-- > 0 && remaining > 0;) {
        EnergyCell& c = m_cells[i];
        if (!c.isCharged()) {
            continue;
        }
        c.charge = 0.0f;
        --remaining;
        HU_LOG_DEBUG("Cells", "Cell %zu discharged for superweapon", i + 1);
    }

    HU_LOG_INFO("Cells", "Consumed %d charged cell(s); %d remain charged",
                cellCount, chargedCellCount());
    return true;
}

void EnergyCellSystem::repairLowestBrokenCell() {
    for (std::size_t i = 0; i < CellCount; ++i) {
        EnergyCell& c = m_cells[i];
        if (!c.broken) {
            continue;
        }
        c.broken = false;
        c.health = c.maxHealth;
        c.charge = 0.0f;
        m_deathLogged = false;
        HU_LOG_INFO("Cells", "Cell %zu REPAIRED (hp %.1f); %zu still broken",
                    i + 1, static_cast<double>(c.health), brokenCellCount());
        return;
    }
    HU_LOG_DEBUG("Cells", "Repair pickup wasted: no broken cells");
}

void EnergyCellSystem::addCharge(float amount) {
    if (amount <= 0.0f) {
        return;
    }
    HU_LOG_DEBUG("Cells", "Pickup charge +%.1f", static_cast<double>(amount));
    routeToCharge(amount);
}

bool EnergyCellSystem::isAlive() const {
    return brokenCellCount() < CellCount;
}

const EnergyCell& EnergyCellSystem::cell(std::size_t i) const {
    if (i >= CellCount) {
        HU_LOG_ERROR("Cells", "cell(%zu) out of range; clamping", i);
        i = CellCount - 1;
    }
    return m_cells[i];
}

std::size_t EnergyCellSystem::brokenCellCount() const {
    std::size_t count = 0;
    for (std::size_t i = 0; i < CellCount; ++i) {
        if (m_cells[i].broken) {
            ++count;
        }
    }
    return count;
}

float EnergyCellSystem::totalHealth() const {
    float total = 0.0f;
    for (std::size_t i = 0; i < CellCount; ++i) {
        total += m_cells[i].health;
    }
    return total;
}

float EnergyCellSystem::totalMaxHealth() const {
    float total = 0.0f;
    for (std::size_t i = 0; i < CellCount; ++i) {
        total += m_cells[i].maxHealth;
    }
    return total;
}

} // namespace hu
