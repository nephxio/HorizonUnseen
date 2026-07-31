#pragma once

// The shared 2D vector type.
//
// This lived in Game/Entity.h back when the legacy scene owned the type
// vocabulary, which meant Core -- the layer with no dependencies -- had to
// include a gameplay header to get it. It belongs here instead: gameplay,
// rendering and UI all speak Vector2, and Core is the only layer all three
// already depend on.
//
// Deliberately left in the global namespace rather than moved into hu:: --
// every layer refers to it unqualified, and the rename would touch far more
// code than it would clean up. Free functions that operate on it live in
// Core/Math.h.

struct Vector2 {
    float x, y;
};
