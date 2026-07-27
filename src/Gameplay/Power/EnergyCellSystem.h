#pragma once

// Energy cells: the player's health bar AND the superweapon fuel tank.
//
// There are five cells, numbered 1..5 in the fiction (0..4 in code). Each has
// hit points, an energy charge, and a damage-rate threshold. Whether incoming
// damage is *absorbed* (routed into charge) or *destructive* (routed into hit
// points) depends on how fast the ship has been taking damage over the last
// five seconds -- not on the size of the individual hit.
//
// Cell 1 is the armoured battery: the most hit points, the smallest charge
// capacity. Cell 5 is the fragile capacitor: the fewest hit points, the largest
// charge capacity. Cells 2-4 interpolate linearly between those endpoints.
//
// Charge fills from the bottom up (cell 1 first); hit points are stripped from
// the top down (cell 5 first). The player dies when all five cells are broken.
//
// This replaces src/Game/HealthSystem.h, which measured an instantaneous
// per-frame damage rate and therefore behaved differently at different frame
// rates.

#include <cstddef>
#include <deque>

namespace hu {

struct EnergyCell {
    float health = 0.0f;
    float maxHealth = 0.0f;
    float charge = 0.0f;
    float maxCharge = 0.0f;
    float damageRateThreshold = 0.0f;
    bool broken = false;

    bool isCharged() const;
};

class EnergyCellSystem {
public:
    static constexpr std::size_t CellCount = 5;

    EnergyCellSystem();

    // Restores every cell to full health and zero charge, and clears the
    // damage window. Call on level start / respawn.
    void reset();

    // Feeds one damage event through the absorb-or-break decision.
    // `currentTime` is the level clock in seconds; it drives the sliding window.
    void applyDamage(float amount, float currentTime);

    // Expires stale entries from the damage window. Safe to call every frame
    // even when no damage arrived; without it the reported rate would only
    // decay when the player is being hit.
    void update(float deltaTime, float currentTime);

    // Number of cells sitting at 100% charge. This selects the superweapon
    // tier via superweaponForCharge().
    int chargedCellCount() const;

    // Spends the charge of `cellCount` charged cells, highest cell first.
    // Returns false and spends nothing when there are not enough charged cells.
    bool consumeCharge(int cellCount);

    // PowerupType::CellRepair. Restores the lowest broken cell to full health
    // (with zero charge). No-op when nothing is broken.
    void repairLowestBrokenCell();

    // PowerupType::EnergyCharge. Pours `amount` into the lowest unfilled cell,
    // overflowing upward through the remaining cells.
    void addCharge(float amount);

    bool isAlive() const;

    // Damage per second over the trailing DamageWindowSeconds.
    float windowedDamageRate() const;

    const EnergyCell& cell(std::size_t i) const;
    std::size_t brokenCellCount() const;

    // Aggregate health, handy for HUD bars and for logging.
    float totalHealth() const;
    float totalMaxHealth() const;

private:
    struct DamageEvent {
        float time = 0.0f;
        float amount = 0.0f;
    };

    void configureCells();
    void pruneWindow(float currentTime);

    // The threshold used for the absorb-or-break decision: the cell that would
    // receive the charge (lowest unfilled unbroken cell), falling back to the
    // highest unbroken cell once everything is full.
    float activeThreshold() const;

    void routeToCharge(float amount);
    void routeToHealth(float amount);

    EnergyCell m_cells[CellCount];
    std::deque<DamageEvent> m_window;
    float m_windowTotal = 0.0f;
    float m_lastTime = 0.0f;
    bool m_deathLogged = false;
};

} // namespace hu
