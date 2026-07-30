"""Evaluates trained policies against a random baseline.

Reports the behavioural metrics that actually say whether the agent learned to
play -- cells preserved, level progress, survival -- rather than only the scalar
it was optimised against.

    python evaluate.py --checkpoint runs/bullethell_v2/checkpoint.pt --episodes 20
"""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import torch

import husim
from train import ActorCritic, RunningNorm


def run_policy(model, normalizer, device, episodes, difficulty, frame_skip,
               seed, greedy, label):
    env = husim.HorizonUnseenEnv(difficulty=difficulty, seed=seed, frame_skip=frame_skip)
    rng = np.random.default_rng(seed)

    rows = []
    for ep in range(episodes):
        obs = env.reset()
        while True:
            if model is None:
                action = int(rng.integers(husim.ACTION_COUNT))
            else:
                normed = normalizer(obs) if normalizer else obs
                tensor = torch.as_tensor(normed, dtype=torch.float32, device=device).unsqueeze(0)
                with torch.no_grad():
                    logits, _ = model(tensor)
                # Greedy shows what the policy believes; sampling shows how it
                # actually behaves during training.
                action = int(torch.argmax(logits, dim=-1).item()) if greedy else \
                    int(torch.distributions.Categorical(logits=logits).sample().item())

            obs, _, done = env.step(action)
            if done:
                break

        info = env.info()
        rows.append(info)

    env.close()

    def mean(field):
        return float(np.mean([r[field] for r in rows]))

    scores = [r["score"] for r in rows]
    print(f"{label:22s} score {mean('score'):8.0f} (best {max(scores):7.0f})  "
          f"cells lost {mean('broken_cells'):4.2f}/5  "
          f"survived {mean('level_time'):6.1f}s  "
          f"progress {mean('progress'):4.2f}  "
          f"grazes {mean('grazes'):6.0f}")
    return rows


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--checkpoint", type=str, action="append", default=[],
                   help="checkpoint(s) to evaluate; may be repeated")
    p.add_argument("--episodes", type=int, default=15)
    p.add_argument("--difficulty", type=int, default=1)
    p.add_argument("--frame-skip", type=int, default=4)
    p.add_argument("--seed", type=int, default=4242)
    p.add_argument("--device", type=str,
                   default="cuda" if torch.cuda.is_available() else "cpu")
    args = p.parse_args()

    device = torch.device(args.device)
    print(f"{args.episodes} episodes each, "
          f"{'bullet hell' if args.difficulty == 1 else 'normal'}\n")

    run_policy(None, None, device, args.episodes, args.difficulty,
               args.frame_skip, args.seed, False, "random baseline")

    for ckpt_path in args.checkpoint:
        path = Path(ckpt_path)
        if not path.is_file():
            print(f"{path} not found, skipping")
            continue

        ckpt = torch.load(path, map_location=device, weights_only=False)
        model = ActorCritic(husim.OBS_SIZE, husim.ACTION_COUNT).to(device)
        model.load_state_dict(ckpt["model"])
        model.eval()

        normalizer = RunningNorm(husim.OBS_SIZE)
        normalizer.load_state_dict(ckpt["normalizer"])

        name = path.parent.name
        run_policy(model, normalizer, device, args.episodes, args.difficulty,
                   args.frame_skip, args.seed, False, f"{name} (sampled)")
        run_policy(model, normalizer, device, args.episodes, args.difficulty,
                   args.frame_skip, args.seed, True, f"{name} (greedy)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
