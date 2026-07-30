"""ctypes bridge to the Horizon Unseen simulation.

The agent trains against the real game: husim.dll is the same GameWorld the
playable build uses, with the renderer and the window removed. Nothing here
reimplements gameplay, so a policy that learns to score well here has learned to
play the actual game.
"""

from __future__ import annotations

import ctypes
import os
import sys
from pathlib import Path

import numpy as np

# Where the build drops husim.dll. Checked in order.
_SEARCH_DIRS = [
    Path(__file__).resolve().parents[2] / "build-agent",
    Path(__file__).resolve().parents[2] / "build",
    Path(__file__).resolve().parents[2] / "build-release",
]

_LIB_NAME = "husim.dll" if sys.platform == "win32" else "libhusim.so"


def _load_library() -> ctypes.CDLL:
    override = os.environ.get("HUSIM_PATH")
    candidates = [Path(override)] if override else []
    candidates += [d / _LIB_NAME for d in _SEARCH_DIRS]

    for path in candidates:
        if path.is_file():
            return ctypes.CDLL(str(path))

    searched = "\n  ".join(str(c) for c in candidates)
    raise FileNotFoundError(
        f"Could not find {_LIB_NAME}. Build it with:\n"
        f"  cmake --build build-agent --target husim\n"
        f"Searched:\n  {searched}"
    )


_lib = _load_library()

_lib.huSimObservationSize.restype = ctypes.c_int
_lib.huSimActionCount.restype = ctypes.c_int
_lib.huSimInfoSize.restype = ctypes.c_int

_lib.huSimCreate.argtypes = [ctypes.c_int, ctypes.c_uint]
_lib.huSimCreate.restype = ctypes.c_void_p

_lib.huSimDestroy.argtypes = [ctypes.c_void_p]

_lib.huSimReset.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_float)]

_lib.huSimStep.argtypes = [
    ctypes.c_void_p,
    ctypes.c_int,
    ctypes.c_int,
    ctypes.POINTER(ctypes.c_float),
    ctypes.POINTER(ctypes.c_int),
]
_lib.huSimStep.restype = ctypes.c_float

_lib.huSimInfo.argtypes = [ctypes.c_void_p, ctypes.POINTER(ctypes.c_float)]

OBS_SIZE = _lib.huSimObservationSize()
ACTION_COUNT = _lib.huSimActionCount()
INFO_SIZE = _lib.huSimInfoSize()

# Field order matches huSimInfo in SimApi.cpp.
INFO_FIELDS = (
    "score",
    "level_time",
    "broken_cells",
    "charged_cells",
    "grazes",
    "progress",
    "enemies",
    "enemy_bullets",
    "boss_health",
    "steps",
)


class HorizonUnseenEnv:
    """A single environment instance."""

    def __init__(self, difficulty: int = 0, seed: int = 0, frame_skip: int = 4):
        self.difficulty = int(difficulty)
        self.seed = int(seed)
        self.frame_skip = int(frame_skip)

        self._handle = _lib.huSimCreate(self.difficulty, self.seed)
        if not self._handle:
            raise RuntimeError("huSimCreate failed")

        # Reused buffers: stepping must not allocate, or Python overhead
        # swamps the simulation it is meant to be driving.
        self._obs = np.zeros(OBS_SIZE, dtype=np.float32)
        self._obs_ptr = self._obs.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
        self._info = np.zeros(INFO_SIZE, dtype=np.float32)
        self._info_ptr = self._info.ctypes.data_as(ctypes.POINTER(ctypes.c_float))
        self._done = ctypes.c_int(0)

    def reset(self) -> np.ndarray:
        _lib.huSimReset(self._handle, self._obs_ptr)
        return self._obs

    def step(self, action: int):
        reward = _lib.huSimStep(
            self._handle, int(action), self.frame_skip, self._obs_ptr,
            ctypes.byref(self._done)
        )
        return self._obs, float(reward), bool(self._done.value)

    def info(self) -> dict:
        _lib.huSimInfo(self._handle, self._info_ptr)
        return dict(zip(INFO_FIELDS, self._info.tolist()))

    def close(self):
        if getattr(self, "_handle", None):
            _lib.huSimDestroy(self._handle)
            self._handle = None

    def __del__(self):
        self.close()


class VecEnv:
    """A batch of independent environments stepped together.

    PPO needs many trajectories per update, and the simulation is fast enough
    that the per-call ctypes overhead is the main cost -- so batching in numpy
    and keeping the Python loop tight matters more than threading.

    Environments auto-reset on termination: the observation returned for a
    finished env is the first observation of its next episode, which is the
    convention PPO's rollout buffer expects.
    """

    def __init__(self, num_envs: int = 16, difficulty: int = 0, seed: int = 0,
                 frame_skip: int = 4):
        self.num_envs = int(num_envs)
        self.envs = [
            HorizonUnseenEnv(difficulty, seed + i, frame_skip)
            for i in range(self.num_envs)
        ]
        self.obs = np.zeros((self.num_envs, OBS_SIZE), dtype=np.float32)
        self.rewards = np.zeros(self.num_envs, dtype=np.float32)
        self.dones = np.zeros(self.num_envs, dtype=bool)

        # Per-episode accumulators, reported when an episode ends.
        self.episode_return = np.zeros(self.num_envs, dtype=np.float64)
        self.episode_length = np.zeros(self.num_envs, dtype=np.int64)

    def reset(self) -> np.ndarray:
        for i, env in enumerate(self.envs):
            self.obs[i] = env.reset()
        self.episode_return[:] = 0.0
        self.episode_length[:] = 0
        return self.obs

    def step(self, actions):
        finished = []
        for i, env in enumerate(self.envs):
            obs, reward, done = env.step(int(actions[i]))
            self.rewards[i] = reward
            self.dones[i] = done
            self.episode_return[i] += reward
            self.episode_length[i] += 1

            if done:
                finished.append({
                    "env": i,
                    "return": float(self.episode_return[i]),
                    "length": int(self.episode_length[i]),
                    **env.info(),
                })
                self.episode_return[i] = 0.0
                self.episode_length[i] = 0
                obs = env.reset()

            self.obs[i] = obs

        return self.obs, self.rewards, self.dones, finished

    def close(self):
        for env in self.envs:
            env.close()
