#pragma once

// The level registry -- the single source of truth for which levels exist.
//
// Everything that needs to enumerate levels (the level select, the secrets
// system, save data, the editor) reads from here. Adding a level means adding
// one LevelDefinition in Levels.cpp; no other system needs to change.

#include "Gameplay/Levels/LevelDefinition.h"

#include <cstddef>
#include <string>
#include <vector>

namespace hu {

class LevelRegistry {
public:
    // Play order. Built once on first use and never mutated afterwards, so the
    // returned reference stays valid for the lifetime of the process.
    static const std::vector<LevelDefinition>& all();

    // Looks a level up by its stable id. Returns nullptr when there is no match.
    static const LevelDefinition* find(const std::string& id);

    static std::size_t count();

    // --- Convenience ------------------------------------------------------
    // Index into all(), or nullptr when out of range.
    static const LevelDefinition* at(std::size_t index);

    // Index of a level by id, or count() when not found.
    static std::size_t indexOf(const std::string& id);
};

} // namespace hu
