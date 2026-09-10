"""
tune_optuna.py - Tune alpha3 va alpha4 bang Optuna + BoTorchSampler
====================================================================
Thiet ke cho 30 phut voi time_limit=100s:
  - 3 instances x 2 seeds = 6 runs/trial
  - 4 parallel workers
  - 12 trials: ceil(12/4) = 3 waves x (6x100s) = 1800s ~ 30 phut

BoTorch (GP + Expected Improvement): hoi tu trong 12 trials,
hieu qua hon TPE khi moi trial dat kem.

Chay:
    python tune_optuna.py               # chuan: 30 phut
    python tune_optuna.py --resume      # tiep tuc neu bi ngat
    python tune_optuna.py --trials 20   # nhieu hon, chinh xac hon
    python tune_optuna.py --no-botorch  # fallback TPE
"""

import argparse
import csv
import os
import re
import subprocess
import sys
import time
from pathlib import Path

import optuna
from optuna.samplers import TPESampler

# ===========================================================================
# CAU HINH
# ===========================================================================

SCRIPT_DIR = Path(__file__).parent.resolve()
EXE        = SCRIPT_DIR / "main.exe"
BASE_DIR   = SCRIPT_DIR.parent.parent

# 3 instances dai dien (du da dang, khong qua ton kem)
INSTANCES = [
    BASE_DIR / "instances/dsj1000-thop/dsj1000_10_usw_10_01.thop",
    BASE_DIR / "instances/dsj1000-thop/dsj1000_10_usw_10_02.thop",
    BASE_DIR / "instances/dsj1000-thop/dsj1000_10_usw_10_03.thop",
]

# 2 seeds (giam variance ma khong tang gap doi thoi gian)
SEEDS = [226489, 326489]

# Tham so co dinh
FIXED = {
    "alpha1":  4.04,
    "alpha2":  7.82,
    "alpha4":  0.795394,  # FIX: ket qua tu tuning alpha4 truoc do
    "eps1":    0.0001,
    "eps2":    0.0001,
    "rho":     0.39,
    "ants":    500,
    "ptries":  2,
    "ls":      1,
    "delta":   1.0,
}

# Chi tune alpha3 (weight exponent trong routing)
ALPHA3_RANGE = (0.0, 10.0)

RE_PROFIT = re.compile(r"Best\s+objective:\s*([\d.eE+\-]+)", re.IGNORECASE)

# ===========================================================================
# CHAY 1 RUN
# ===========================================================================

def run_single(instance: Path, seed: int, alpha3: float, alpha4: float,
               time_limit: int) -> float:
    tmp_sol = SCRIPT_DIR / f"_tmp_{os.getpid()}_{seed}.sol"
    cmd = [
        str(EXE), str(instance),
        "--ants",           str(FIXED["ants"]),
        "--time-limit",     str(time_limit),
        "--local-search",   str(FIXED["ls"]),
        "--alpha1",         str(FIXED["alpha1"]),
        "--alpha2",         str(FIXED["alpha2"]),
        "--alpha3",         str(alpha3),
        "--alpha4",         str(alpha4),
        "--eps1",           str(FIXED["eps1"]),
        "--eps2",           str(FIXED["eps2"]),
        "--rho",            str(FIXED["rho"]),
        "--delta",          str(FIXED["delta"]),
        "--seed",           str(seed),
        "--output",         str(tmp_sol),
        "--ptries",         str(FIXED["ptries"]),
        "--step-online",    "0",
        "--delayed-online", "0",
    ]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              timeout=time_limit + 30)
        output = proc.stdout + "\n" + proc.stderr
        m = RE_PROFIT.search(output)
        if m:
            return float(m.group(1))
        return 0.0
    except (subprocess.TimeoutExpired, ValueError, OSError):
        return 0.0
    finally:
        try:
            tmp_sol.unlink(missing_ok=True)
        except Exception:
            pass


# ===========================================================================
# OBJECTIVE
# ===========================================================================

def make_objective(time_limit: int):
    valid = [i for i in INSTANCES if i.exists()]
    def objective(trial: optuna.Trial) -> float:
        alpha3 = trial.suggest_float("alpha3", *ALPHA3_RANGE)
        alpha4 = FIXED["alpha4"]   # = 0.795394, fixed
        profits = []
        for inst in valid:
            for seed in SEEDS:
                profits.append(run_single(inst, seed, alpha3, alpha4, time_limit))
        mean = sum(profits) / len(profits) if profits else 0.0
        return -mean
    return objective


# ===========================================================================
# SAMPLER
# ===========================================================================

def make_sampler(use_botorch: bool, n_startup: int):
    if use_botorch:
        try:
            from optuna_integration import BoTorchSampler
            return "BoTorchSampler (GP + Expected Improvement)", BoTorchSampler(
                n_startup_trials=n_startup, seed=42
            )
        except ImportError:
            pass
    return "TPESampler", TPESampler(n_startup_trials=n_startup, seed=42)


# ===========================================================================
# MAIN
# ===========================================================================

def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--trials",     type=int, default=12,
                   help="So trials (default: 12 ~ 30 phut voi 4 workers)")
    p.add_argument("--time-limit", type=int, default=100,
                   help="Giay moi run (default: 100)")
    p.add_argument("--jobs",       type=int, default=4,
                   help="Parallel workers (default: 4)")
    p.add_argument("--startup",    type=int, default=5,
                   help="So trial random truoc GP (default: 5)")
    p.add_argument("--study-name", type=str, default="alpha34_botorch")
    p.add_argument("--db",         type=str, default="optuna_study.db")
    p.add_argument("--resume",     action="store_true")
    p.add_argument("--no-botorch", dest="use_botorch", action="store_false", default=True)
    return p.parse_args()


def main():
    args = parse_args()
    optuna.logging.set_verbosity(optuna.logging.WARNING)

    if not EXE.exists():
        print(f"[ERROR] Khong tim thay: {EXE}")
        sys.exit(1)

    valid_inst = [i for i in INSTANCES if i.exists()]
    if not valid_inst:
        print("[ERROR] Khong co instance nao hop le.")
        sys.exit(1)

    runs_per_trial = len(valid_inst) * len(SEEDS)
    # Wall time: ceil(trials/jobs) waves, moi wave chay runs_per_trial*time_limit giay
    import math
    waves     = math.ceil(args.trials / args.jobs)
    est_sec   = waves * runs_per_trial * args.time_limit
    est_min   = est_sec / 60

    sampler_name, sampler = make_sampler(args.use_botorch, args.startup)
    storage  = f"sqlite:///{args.db}"

    if not args.resume:
        try:
            optuna.delete_study(study_name=args.study_name, storage=storage)
        except Exception:
            pass

    study = optuna.create_study(
        study_name=args.study_name,
        storage=storage,
        load_if_exists=args.resume,
        direction="minimize",
        sampler=sampler,
    )

    # CSV
    csv_path = SCRIPT_DIR / "optuna_trials.csv"
    new_file = not args.resume or not csv_path.exists()
    csv_file = open(csv_path, "a", newline="", encoding="utf-8")
    writer   = csv.writer(csv_file)
    if new_file:
        writer.writerow(["trial", "alpha4",
                         "mean_profit", "best_so_far", "timestamp"])

    # Header
    sep = "=" * 70
    print(sep)
    print("  Optuna + BoTorch Tuning  |  not-simple-2-tuneopt")
    print(sep)
    print(f"  Sampler     : {sampler_name}")
    print(f"  EXE         : {EXE.name}")
    print(f"  Instances   : {len(valid_inst)} x {len(SEEDS)} seeds = {runs_per_trial} runs/trial")
    print(f"  time-limit  : {args.time_limit}s/run")
    print(f"  Workers     : {args.jobs}  |  Trials: {args.trials}  |  Waves: {waves}")
    print(f"  Est. time   : ~{est_min:.0f} min  (~{est_min/60:.1f}h)")
    print(f"  GP startup  : {args.startup} random trials truoc khi GP bat dau")
    print(f"  Resume      : {args.resume}  |  DB: {args.db}")
    print(f"  Fixed       : alpha1={FIXED['alpha1']} alpha2={FIXED['alpha2']} "
          f"alpha4={FIXED['alpha4']} rho={FIXED['rho']} "
          f"ants={FIXED['ants']} ptries={FIXED['ptries']} ls={FIXED['ls']}")
    print(f"  Tuning      : alpha3 in {ALPHA3_RANGE}  (1D search)")
    print(sep)
    print(f"\n{'Trial':>6}  {'alpha3':>8}  "
          f"{'profit':>13}  {'best':>13}  elapsed  note")
    print("-" * 58)

    t0    = time.time()
    state = {"best": float("-inf")}

    def callback(study: optuna.Study, trial: optuna.trial.FrozenTrial):
        profit = -trial.value
        is_best = profit > state["best"]
        if is_best:
            state["best"] = profit
        elapsed = time.time() - t0
        hh, r = divmod(int(elapsed), 3600)
        mm, ss = divmod(r, 60)
        note = " << BEST" if is_best else ""
        print(
            f"{trial.number:>6}  {trial.params['alpha3']:>8.4f}  "
            f"{profit:>13.0f}  {state['best']:>13.0f}  "
            f"{hh:02d}:{mm:02d}:{ss:02d}{note}",
            flush=True,
        )
        writer.writerow([
            trial.number,
            f"{trial.params['alpha3']:.6f}",
            f"{profit:.2f}",
            f"{state['best']:.2f}",
            time.strftime("%Y-%m-%d %H:%M:%S"),
        ])
        csv_file.flush()

    study.optimize(
        make_objective(args.time_limit),
        n_trials=args.trials,
        n_jobs=args.jobs,
        callbacks=[callback],
        show_progress_bar=False,
    )

    csv_file.close()

    best    = study.best_trial
    best_a3 = best.params["alpha3"]
    wall    = time.time() - t0

    print("\n" + sep)
    print("  KET QUA")
    print(sep)
    print(f"  Best trial   : #{best.number}")
    print(f"  alpha3       : {best_a3:.6f}")
    print(f"  alpha4       : {FIXED['alpha4']}  (fixed)")
    print(f"  Mean profit  : {-best.value:.0f}  (avg {runs_per_trial} runs)")
    print(f"  Wall time    : {wall/60:.1f} min")
    print()
    print("  Lenh benchmark (998s, copy-paste):")
    print(f"  .\\run.ps1 ..\\..\\instances\\dsj1000-thop\\dsj1000_10_usw_10_03.thop `")
    print(f"    -Ants {FIXED['ants']} -TimeLimit 998 -LocalSearch {FIXED['ls']} `")
    print(f"    -Alpha1 {FIXED['alpha1']} -Alpha2 {FIXED['alpha2']} `")
    print(f"    -Alpha3 {best_a3:.6f} -Alpha4 {FIXED['alpha4']} `")
    print(f"    -Eps1 {FIXED['eps1']} -Eps2 {FIXED['eps2']} -Rho {FIXED['rho']} `")
    print(f"    -Seed 642580 -OutputFile out.sol -LogEnabled 0 -PTries {FIXED['ptries']}")
    print(sep)


if __name__ == "__main__":
    main()
