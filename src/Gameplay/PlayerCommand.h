#pragma once

// One frame of player intent.
//
// GameWorld used to read the keyboard directly through InputSystem, which
// bound the simulation to GLFW and meant it could not be advanced without a
// window. Intent is now expressed as plain data: the application translates key
// state into a command, and an AI agent produces one directly. The simulation
// cannot tell the difference, which is what makes headless training possible.

namespace hu {

struct PlayerCommand {
    // Desired movement direction. Magnitude beyond 1 is clamped, so a caller
    // can pass raw axis sums without normalising first.
    float moveX = 0.0f;
    float moveY = 0.0f;   // Negative is up: world space is y-down.

    // Held, not edge-triggered: the weapon system owns its own cadence.
    bool fire = false;

    // Edge-triggered. The caller is responsible for presenting this as a single
    // frame pulse rather than a held state.
    bool fireSuperweapon = false;

    // -1 cycles back, +1 forward, 0 leaves the weapon alone. Also edge-triggered.
    int cycleWeapon = 0;
};

} // namespace hu
