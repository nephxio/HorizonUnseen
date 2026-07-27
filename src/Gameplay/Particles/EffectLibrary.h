#pragma once

// Named visual effects.
//
// Gameplay never builds a ParticleSpec by hand: it posts an EffectRequest
// (kind + position + direction + scale + tint) and this translates that into
// one or more emissions. Keeping the recipes in a single file means the whole
// visual vocabulary of the game can be tuned in one place, and every call site
// stays a single line.

#include "Gameplay/IGameWorld.h"   // EffectKind, EffectRequest

namespace hu {

class ParticleSystem;

// Services every EffectKind. Unknown kinds fall back to Impact so a new enum
// entry is visible on screen rather than silently invisible.
//
// request.scale multiplies both particle size and particle count;
// request.tint is the base colour the effect is built around.
void playEffect(ParticleSystem& particles, const EffectRequest& request);

} // namespace hu
