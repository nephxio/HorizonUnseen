#pragma once

// Flat C ABI over the game simulation, for reinforcement learning.
//
// Exposed as a shared library and driven from Python via ctypes, so the agent
// trains against the real game rather than a reimplementation of it. There is
// no renderer, no window and no fixed frame rate: the world is advanced at a
// fixed timestep as fast as the CPU allows.
//
// Everything here is plain C types on purpose. No C++ objects cross the
// boundary, so the Python side needs nothing but ctypes.

#include <cstdint>

#if defined(_WIN32)
#  define HU_SIM_API extern "C" __declspec(dllexport)
#else
#  define HU_SIM_API extern "C" __attribute__((visibility("default")))
#endif

// Opaque environment handle.
typedef void* HuSimHandle;

// ---------------------------------------------------------------------------
// Layout constants. The Python wrapper reads these rather than hard-coding, so
// the observation can be extended without the two sides drifting apart.
// ---------------------------------------------------------------------------

HU_SIM_API int huSimObservationSize();
HU_SIM_API int huSimActionCount();
HU_SIM_API int huSimInfoSize();

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

// difficulty: 0 = normal, 1 = bullet hell.
// seed: seeds the effect/scatter randomness so runs can be reproduced.
HU_SIM_API HuSimHandle huSimCreate(int difficulty, unsigned int seed);
HU_SIM_API void huSimDestroy(HuSimHandle handle);

// Restarts the level. `observation` receives huSimObservationSize() floats.
HU_SIM_API void huSimReset(HuSimHandle handle, float* observation);

// Advances the world by `frameSkip` fixed 1/60s ticks with `action` held.
//
// Returns the reward for the step. `done` is set when the run ended, and
// `observation` receives the new state.
HU_SIM_API float huSimStep(HuSimHandle handle,
                           int action,
                           int frameSkip,
                           float* observation,
                           int* done);

// Diagnostic counters for logging and evaluation; see SimApi.cpp for the
// field order.
HU_SIM_API void huSimInfo(HuSimHandle handle, float* info);
