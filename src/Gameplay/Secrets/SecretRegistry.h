#pragma once

// Owns every secret definition in the game.
//
// The registry is the single source of truth: the tracker, the save system and
// the secrets UI all derive their totals from here by iterating
// LevelRegistry::all(). Adding a level with its secrets therefore requires no
// edit anywhere else.

#include "Gameplay/Secrets/SecretDefinition.h"

#include <cstddef>
#include <string>
#include <vector>

// Totals are derived by walking every registered level, so the two registries
// stay in step automatically.
#include "Gameplay/Levels/Levels.h"

namespace hu {

class SecretRegistry {
public:
    // Every secret in the game, in declaration order.
    static const std::vector<SecretDefinition>& all();

    // Secrets belonging to one level. Pointers are stable for the process
    // lifetime (the backing storage is a function-local static).
    static std::vector<const SecretDefinition*> forLevel(const std::string& levelId);

    // Number of secrets in one level.
    static std::size_t countForLevel(const std::string& levelId);

    // Total number of secrets across every *registered* level. Derived by
    // iterating LevelRegistry::all(), so secrets attached to a level id that is
    // not registered are intentionally not counted towards completion.
    static std::size_t totalCount();

    // Lookup by globally unique secret id, or nullptr.
    static const SecretDefinition* find(const std::string& secretId);
};

} // namespace hu
