#!/usr/bin/env python3
"""Headless gameplay regression test for CI.

Runs the real simulation on a machine with no GPU and no display, and asserts
the things that would otherwise break silently. This is deliberately fast and
strict, unlike smoke_test.py which is exploratory and prints throughput.

Exits non-zero with a specific message on the first failure.
"""

from __future__ import annotations

import sys

import numpy as np

import husim

FAILURES: list[str] = []


def check(condition: bool, message: str) -> None:
    if condition:
        print(f"  PASS  {message}")
    else:
        print(f"  FAIL  {message}")
        FAILURES.append(message)


def test_interface() -> None:
    print("\ninterface:")
    check(husim.OBS_SIZE > 0, f"observation size is positive ({husim.OBS_SIZE})")
    check(husim.ACTION_COUNT > 1, f"action count is usable ({husim.ACTION_COUNT})")
    check(len(husim.INFO_FIELDS) == husim.INFO_SIZE,
          "info field names match the C side's reported size")


def test_episode(difficulty: int, label: str) -> dict:
    """Plays one episode with a fixed action sequence and checks it behaves."""
    print(f"\n{label}:")
    env = husim.HorizonUnseenEnv(difficulty=difficulty, seed=12345, frame_skip=4)
    obs = env.reset()

    check(obs.shape == (husim.OBS_SIZE,), "reset returns a correctly shaped observation")
    check(bool(np.all(np.isfinite(obs))), "reset observation is finite")

    rng = np.random.default_rng(99)
    steps = 0
    non_finite = False
    saw_bullets = False
    took_damage = False
    max_steps = 6000

    while steps < max_steps:
        obs, _, done = env.step(int(rng.integers(husim.ACTION_COUNT)))
        steps += 1

        if not np.all(np.isfinite(obs)):
            non_finite = True
            break

        info = env.info()
        if info["enemy_bullets"] > 0:
            saw_bullets = True
        if info["broken_cells"] > 0 or info["charged_cells"] > 0:
            # Either routing path having fired proves damage reached the cells.
            took_damage = True

        if done:
            break

    info = env.info()
    check(not non_finite, "observations stay finite for the whole episode")
    check(steps < max_steps, f"episode terminates on its own ({steps} steps)")
    check(info["level_time"] > 10.0, f"level clock advances ({info['level_time']:.0f}s)")
    check(info["progress"] > 0.05, f"level makes progress ({info['progress']:.2f})")
    check(saw_bullets, "enemies spawn and fire")
    check(took_damage, "damage reaches the energy cells")
    check(info["score"] > 0, f"score accrues ({info['score']:.0f})")

    env.close()
    return info


def test_determinism() -> None:
    """Same seed and same actions must produce the same outcome.

    Non-determinism here would make every training result and every bug report
    irreproducible, and it is the kind of thing that creeps in unnoticed.
    """
    print("\ndeterminism:")
    actions = np.random.default_rng(7).integers(husim.ACTION_COUNT, size=900)

    results = []
    for _ in range(2):
        env = husim.HorizonUnseenEnv(difficulty=1, seed=2024, frame_skip=4)
        env.reset()
        total = 0.0
        for action in actions:
            _, reward, done = env.step(int(action))
            total += reward
            if done:
                break
        results.append((round(total, 3), env.info()["score"]))
        env.close()

    check(results[0] == results[1],
          f"identical seed and actions reproduce the run {results[0]} vs {results[1]}")


def test_reset_clears_state() -> None:
    print("\nreset:")
    env = husim.HorizonUnseenEnv(difficulty=1, seed=5, frame_skip=4)
    env.reset()
    rng = np.random.default_rng(3)
    for _ in range(600):
        _, _, done = env.step(int(rng.integers(husim.ACTION_COUNT)))
        if done:
            break

    dirty = env.info()
    env.reset()
    clean = env.info()

    check(clean["score"] == 0, "reset clears score")
    check(clean["level_time"] < dirty["level_time"], "reset rewinds the level clock")
    check(clean["broken_cells"] == 0, "reset restores the energy cells")
    env.close()


def main() -> int:
    print(f"husim loaded: {husim.OBS_SIZE} observations, {husim.ACTION_COUNT} actions")

    test_interface()
    test_episode(0, "normal difficulty")
    test_episode(1, "bullet hell")
    test_determinism()
    test_reset_clears_state()

    print()
    if FAILURES:
        print(f"{len(FAILURES)} check(s) failed:")
        for f in FAILURES:
            print(f"  - {f}")
        return 1

    print("all gameplay checks passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
