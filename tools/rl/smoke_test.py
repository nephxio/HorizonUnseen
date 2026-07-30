"""Sanity-checks the RL environment before any training is attempted.

Verifies the bridge works, the observation is well-formed, episodes terminate,
and -- most importantly -- measures throughput. Training is only feasible if the
simulation can be stepped tens of thousands of times per second.
"""

from __future__ import annotations

import time

import numpy as np

import husim


def main() -> int:
    print(f"observation size : {husim.OBS_SIZE}")
    print(f"action count     : {husim.ACTION_COUNT}")

    # --- Single env, one episode -----------------------------------------
    env = husim.HorizonUnseenEnv(difficulty=1, seed=1, frame_skip=4)
    obs = env.reset()

    assert obs.shape == (husim.OBS_SIZE,), obs.shape
    assert np.all(np.isfinite(obs)), "observation contains NaN/inf"
    print(f"\nreset obs range  : [{obs.min():.3f}, {obs.max():.3f}]")

    rng = np.random.default_rng(0)
    total = 0.0
    steps = 0
    while True:
        obs, reward, done = env.step(rng.integers(husim.ACTION_COUNT))
        total += reward
        steps += 1
        if not np.all(np.isfinite(obs)):
            print(f"  NON-FINITE OBSERVATION at step {steps}")
            return 1
        if done:
            break

    info = env.info()
    print(f"random episode   : {steps} steps, return {total:.1f}")
    print(f"  score {info['score']:.0f}, survived {info['level_time']:.1f}s, "
          f"{info['broken_cells']:.0f} cells broken, {info['grazes']:.0f} grazes")
    env.close()

    # --- Throughput -------------------------------------------------------
    for num_envs in (1, 16, 64):
        vec = husim.VecEnv(num_envs=num_envs, difficulty=1, seed=100, frame_skip=4)
        vec.reset()
        actions = np.zeros(num_envs, dtype=np.int64)

        n_iters = 400
        start = time.perf_counter()
        for _ in range(n_iters):
            actions[:] = rng.integers(husim.ACTION_COUNT, size=num_envs)
            vec.step(actions)
        elapsed = time.perf_counter() - start

        agent_steps = n_iters * num_envs
        sim_frames = agent_steps * 4          # frame_skip
        print(f"\n{num_envs:3d} envs: {agent_steps / elapsed:9.0f} agent steps/s   "
              f"{sim_frames / elapsed:10.0f} sim frames/s "
              f"({sim_frames / elapsed / 60:.0f}x realtime)")
        vec.close()

    # --- Does a trivial policy differ from random? ------------------------
    # If "always sit still" scores the same as random, the reward signal is not
    # measuring anything useful and training would be pointless.
    for label, policy in (("random", None), ("hold still", 0), ("always up", 1)):
        vec = husim.VecEnv(num_envs=16, difficulty=1, seed=7, frame_skip=4)
        vec.reset()
        returns = []
        actions = np.zeros(16, dtype=np.int64)
        for _ in range(1500):
            if policy is None:
                actions[:] = rng.integers(husim.ACTION_COUNT, size=16)
            else:
                actions[:] = policy
            _, _, _, finished = vec.step(actions)
            returns.extend(f["return"] for f in finished)
        if returns:
            print(f"\n{label:11s}: {len(returns):3d} episodes, "
                  f"mean return {np.mean(returns):8.2f}, best {np.max(returns):8.2f}")
        else:
            print(f"\n{label:11s}: no episodes finished")
        vec.close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
