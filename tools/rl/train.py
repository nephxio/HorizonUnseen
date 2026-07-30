"""PPO training for Horizon Unseen.

Trains an actor-critic policy to maximise score against the real game
simulation. Run with:

    python train.py --steps 2000000 --envs 16

Checkpoints and a CSV of training progress are written to tools/rl/runs/.
"""

from __future__ import annotations

import argparse
import csv
import time
from pathlib import Path

import numpy as np
import torch
import torch.nn as nn

import husim


# ---------------------------------------------------------------------------
# Running observation normalisation
#
# The observation mixes quantities on very different scales (normalised
# positions near 0, one-hot flags at exactly 1, health fractions in [0,1]).
# Feeding that to an MLP raw makes early training slow and unstable, so it is
# whitened with statistics accumulated as training proceeds.
# ---------------------------------------------------------------------------
class RunningNorm:
    def __init__(self, size: int, epsilon: float = 1e-4):
        self.mean = np.zeros(size, dtype=np.float64)
        self.var = np.ones(size, dtype=np.float64)
        self.count = epsilon

    def update(self, batch: np.ndarray) -> None:
        batch_mean = batch.mean(axis=0)
        batch_var = batch.var(axis=0)
        batch_count = batch.shape[0]

        delta = batch_mean - self.mean
        total = self.count + batch_count

        self.mean += delta * batch_count / total
        m_a = self.var * self.count
        m_b = batch_var * batch_count
        self.var = (m_a + m_b + delta**2 * self.count * batch_count / total) / total
        self.count = total

    def __call__(self, obs: np.ndarray) -> np.ndarray:
        return np.clip((obs - self.mean) / np.sqrt(self.var + 1e-8), -10.0, 10.0)

    def state_dict(self) -> dict:
        return {"mean": self.mean, "var": self.var, "count": self.count}

    def load_state_dict(self, state: dict) -> None:
        self.mean = state["mean"]
        self.var = state["var"]
        self.count = state["count"]


# ---------------------------------------------------------------------------
# Policy
# ---------------------------------------------------------------------------
def layer_init(layer: nn.Linear, std: float = np.sqrt(2), bias: float = 0.0) -> nn.Linear:
    # Orthogonal init with a small final-layer gain is the standard PPO recipe:
    # it keeps the initial policy close to uniform so early exploration is broad.
    torch.nn.init.orthogonal_(layer.weight, std)
    torch.nn.init.constant_(layer.bias, bias)
    return layer


class ActorCritic(nn.Module):
    def __init__(self, obs_size: int, action_count: int, hidden: int = 256):
        super().__init__()
        self.trunk = nn.Sequential(
            layer_init(nn.Linear(obs_size, hidden)), nn.Tanh(),
            layer_init(nn.Linear(hidden, hidden)), nn.Tanh(),
        )
        self.policy_head = layer_init(nn.Linear(hidden, action_count), std=0.01)
        self.value_head = layer_init(nn.Linear(hidden, 1), std=1.0)

    def forward(self, obs: torch.Tensor):
        features = self.trunk(obs)
        return self.policy_head(features), self.value_head(features).squeeze(-1)

    def act(self, obs: torch.Tensor):
        logits, value = self(obs)
        dist = torch.distributions.Categorical(logits=logits)
        action = dist.sample()
        return action, dist.log_prob(action), dist.entropy(), value

    def evaluate(self, obs: torch.Tensor, action: torch.Tensor):
        logits, value = self(obs)
        dist = torch.distributions.Categorical(logits=logits)
        return dist.log_prob(action), dist.entropy(), value


# ---------------------------------------------------------------------------
# Training
# ---------------------------------------------------------------------------
def parse_args() -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Train a PPO agent on Horizon Unseen")
    p.add_argument("--steps", type=int, default=2_000_000, help="total agent steps")
    p.add_argument("--envs", type=int, default=16)
    p.add_argument("--rollout", type=int, default=128, help="steps per env per update")
    p.add_argument("--epochs", type=int, default=4)
    p.add_argument("--minibatches", type=int, default=4)
    p.add_argument("--lr", type=float, default=3e-4)
    p.add_argument("--gamma", type=float, default=0.995)
    p.add_argument("--gae-lambda", type=float, default=0.95)
    p.add_argument("--clip", type=float, default=0.2)
    p.add_argument("--entropy", type=float, default=0.01)
    p.add_argument("--value-coef", type=float, default=0.5)
    p.add_argument("--max-grad-norm", type=float, default=0.5)
    p.add_argument("--difficulty", type=int, default=1, help="0 normal, 1 bullet hell")
    p.add_argument("--frame-skip", type=int, default=4)
    p.add_argument("--seed", type=int, default=0)
    p.add_argument("--device", type=str, default="cuda" if torch.cuda.is_available() else "cpu")
    p.add_argument("--run-name", type=str, default=None)
    p.add_argument("--resume", type=str, default=None, help="checkpoint to resume from")
    return p.parse_args()


def main() -> int:
    args = parse_args()

    torch.manual_seed(args.seed)
    np.random.seed(args.seed)

    run_name = args.run_name or time.strftime("run_%Y%m%d_%H%M%S")
    run_dir = Path(__file__).resolve().parent / "runs" / run_name
    run_dir.mkdir(parents=True, exist_ok=True)

    device = torch.device(args.device)
    print(f"run       : {run_name}")
    print(f"device    : {device}")
    print(f"obs/action: {husim.OBS_SIZE} / {husim.ACTION_COUNT}")
    print(f"difficulty: {'bullet hell' if args.difficulty == 1 else 'normal'}")

    envs = husim.VecEnv(num_envs=args.envs, difficulty=args.difficulty,
                        seed=args.seed * 1000, frame_skip=args.frame_skip)

    model = ActorCritic(husim.OBS_SIZE, husim.ACTION_COUNT).to(device)
    optimizer = torch.optim.Adam(model.parameters(), lr=args.lr, eps=1e-5)
    normalizer = RunningNorm(husim.OBS_SIZE)

    start_update = 0
    if args.resume:
        ckpt = torch.load(args.resume, map_location=device, weights_only=False)
        model.load_state_dict(ckpt["model"])
        optimizer.load_state_dict(ckpt["optimizer"])
        normalizer.load_state_dict(ckpt["normalizer"])
        start_update = ckpt.get("update", 0)
        print(f"resumed from {args.resume} at update {start_update}")

    batch_size = args.envs * args.rollout
    minibatch_size = batch_size // args.minibatches
    total_updates = max(1, args.steps // batch_size)

    # Rollout buffers, allocated once.
    obs_buf = torch.zeros((args.rollout, args.envs, husim.OBS_SIZE), device=device)
    act_buf = torch.zeros((args.rollout, args.envs), dtype=torch.long, device=device)
    logp_buf = torch.zeros((args.rollout, args.envs), device=device)
    rew_buf = torch.zeros((args.rollout, args.envs), device=device)
    done_buf = torch.zeros((args.rollout, args.envs), device=device)
    val_buf = torch.zeros((args.rollout, args.envs), device=device)

    raw_obs = envs.reset()
    normalizer.update(raw_obs)
    next_obs = torch.as_tensor(normalizer(raw_obs), dtype=torch.float32, device=device)
    next_done = torch.zeros(args.envs, device=device)

    csv_path = run_dir / "progress.csv"
    csv_file = csv_path.open("w", newline="")
    writer = csv.writer(csv_file)
    writer.writerow(["update", "agent_steps", "elapsed_s", "steps_per_s",
                     "mean_return", "mean_score", "mean_length",
                     "mean_grazes", "mean_progress", "best_score",
                     "policy_loss", "value_loss", "entropy"])

    recent_returns: list[float] = []
    recent_scores: list[float] = []
    recent_lengths: list[int] = []
    recent_grazes: list[float] = []
    recent_progress: list[float] = []
    best_score = 0.0

    start_time = time.perf_counter()
    global_step = 0

    for update in range(start_update, total_updates):
        # Linear learning-rate decay: standard PPO practice, and it matters
        # here because early policies change fast and late ones should not.
        frac = 1.0 - update / total_updates
        for group in optimizer.param_groups:
            group["lr"] = args.lr * frac

        # --- Collect rollout ---------------------------------------------
        for step in range(args.rollout):
            obs_buf[step] = next_obs
            done_buf[step] = next_done

            with torch.no_grad():
                action, logp, _, value = model.act(next_obs)

            act_buf[step] = action
            logp_buf[step] = logp
            val_buf[step] = value

            raw_obs, rewards, dones, finished = envs.step(action.cpu().numpy())
            global_step += args.envs

            normalizer.update(raw_obs)
            next_obs = torch.as_tensor(normalizer(raw_obs), dtype=torch.float32, device=device)
            next_done = torch.as_tensor(dones.astype(np.float32), device=device)
            rew_buf[step] = torch.as_tensor(rewards, device=device)

            for ep in finished:
                recent_returns.append(ep["return"])
                recent_scores.append(ep["score"])
                recent_lengths.append(ep["length"])
                recent_grazes.append(ep["grazes"])
                recent_progress.append(ep["progress"])
                best_score = max(best_score, ep["score"])

        # --- GAE ----------------------------------------------------------
        with torch.no_grad():
            _, next_value = model(next_obs)
            advantages = torch.zeros_like(rew_buf)
            last_gae = 0.0
            for t in reversed(range(args.rollout)):
                if t == args.rollout - 1:
                    next_nonterminal = 1.0 - next_done
                    next_values = next_value
                else:
                    next_nonterminal = 1.0 - done_buf[t + 1]
                    next_values = val_buf[t + 1]
                delta = rew_buf[t] + args.gamma * next_values * next_nonterminal - val_buf[t]
                last_gae = delta + args.gamma * args.gae_lambda * next_nonterminal * last_gae
                advantages[t] = last_gae
            returns = advantages + val_buf

        b_obs = obs_buf.reshape(-1, husim.OBS_SIZE)
        b_act = act_buf.reshape(-1)
        b_logp = logp_buf.reshape(-1)
        b_adv = advantages.reshape(-1)
        b_ret = returns.reshape(-1)

        # --- Optimise -----------------------------------------------------
        indices = np.arange(batch_size)
        policy_loss = value_loss = entropy_loss = 0.0

        for _ in range(args.epochs):
            np.random.shuffle(indices)
            for start in range(0, batch_size, minibatch_size):
                mb = indices[start:start + minibatch_size]

                new_logp, entropy, new_value = model.evaluate(b_obs[mb], b_act[mb])
                log_ratio = new_logp - b_logp[mb]
                ratio = log_ratio.exp()

                # Advantage normalisation per minibatch.
                mb_adv = b_adv[mb]
                mb_adv = (mb_adv - mb_adv.mean()) / (mb_adv.std() + 1e-8)

                pg_loss = torch.max(
                    -mb_adv * ratio,
                    -mb_adv * torch.clamp(ratio, 1 - args.clip, 1 + args.clip),
                ).mean()

                v_loss = 0.5 * ((new_value - b_ret[mb]) ** 2).mean()
                ent = entropy.mean()

                loss = pg_loss - args.entropy * ent + args.value_coef * v_loss

                optimizer.zero_grad()
                loss.backward()
                nn.utils.clip_grad_norm_(model.parameters(), args.max_grad_norm)
                optimizer.step()

                policy_loss, value_loss, entropy_loss = (
                    pg_loss.item(), v_loss.item(), ent.item()
                )

        # --- Report -------------------------------------------------------
        elapsed = time.perf_counter() - start_time
        sps = global_step / elapsed

        def tail_mean(values, n=40):
            return float(np.mean(values[-n:])) if values else float("nan")

        row = [update, global_step, round(elapsed, 1), round(sps, 1),
               round(tail_mean(recent_returns), 2),
               round(tail_mean(recent_scores), 1),
               round(tail_mean(recent_lengths), 1),
               round(tail_mean(recent_grazes), 1),
               round(tail_mean(recent_progress), 4),
               round(best_score, 1),
               round(policy_loss, 4), round(value_loss, 4), round(entropy_loss, 4)]
        writer.writerow(row)
        csv_file.flush()

        if update % 5 == 0 or update == total_updates - 1:
            print(f"upd {update:5d} | steps {global_step:9d} | {sps:6.0f}/s | "
                  f"return {tail_mean(recent_returns):8.2f} | "
                  f"score {tail_mean(recent_scores):8.0f} | "
                  f"len {tail_mean(recent_lengths):6.0f} | "
                  f"prog {tail_mean(recent_progress):.2f} | "
                  f"best {best_score:7.0f} | H {entropy_loss:.3f}")

        if update % 25 == 0 or update == total_updates - 1:
            torch.save({
                "model": model.state_dict(),
                "optimizer": optimizer.state_dict(),
                "normalizer": normalizer.state_dict(),
                "update": update,
                "args": vars(args),
            }, run_dir / "checkpoint.pt")

    csv_file.close()
    envs.close()
    print(f"\ndone. best score {best_score:.0f}. artefacts in {run_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
