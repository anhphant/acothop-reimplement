#!/usr/bin/env python3
"""Run the ESACO THOP solver over all instances under `instances/`.

Parameters default to the ACO++ sample/default (MMAS) set, seed=10, and a
per-instance time-limit under 15 minutes. Every value can be overridden by an
environment variable.

Examples:
    # everything (all 660 instances)
    python3 run_all.py

    # quick smoke: 2 pr107 instances, 10s each
    FILTER=pr107 MAX_INSTANCES=2 TIME_LIMIT=10 python3 run_all.py
"""
import os
import re
import csv
import glob
import subprocess
from pathlib import Path


def env(name, default):
    return os.environ.get(name, default)


# --- ACO++ sample/default (MMAS) parameter set ---
SEED         = env("SEED", "10")
TIME_LIMIT   = env("TIME_LIMIT", "840")     # seconds; < 900 = under 15 min
ANTS         = env("ANTS", "10")            # aco++ default n_ants
ALPHA        = env("ALPHA", "1.0")          # pheromone weight
BETA         = env("BETA", "2.0")           # heuristic weight
RHO          = env("RHO", "0.5")            # evaporation
LOCAL_SEARCH = env("LOCAL_SEARCH", "0")     # 0: off (LS n*n matrix OOMs for big instances)
NN_ANTS      = env("NN_ANTS", "20")
NN_LS        = env("NN_LS", "20")
PTRIES       = env("PTRIES", "1")

# --- ESACO switches (S1/S2/S3) ---
ESACO  = env("ESACO", "1")
ESACO2 = env("ESACO2", "1")
ESACO3 = env("ESACO3", "1")

# --- paths / scope ---
INSTANCES_DIR = Path(env("INSTANCES_DIR", "/app/instances"))
OUT_DIR       = Path(env("OUT_DIR", "/app/results"))
BIN           = env("BIN", "/app/esaco")
FILTER        = env("FILTER", "")           # substring filter on the instance filename
MAX_INSTANCES = int(env("MAX_INSTANCES", "0") or "0")   # 0 = all


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    files = sorted(glob.glob(str(INSTANCES_DIR / "*-thop" / "*.thop")))
    if FILTER:
        files = [f for f in files if FILTER in os.path.basename(f)]
    if MAX_INSTANCES:
        files = files[:MAX_INSTANCES]

    print(f"instances={len(files)} seed={SEED} time_limit={TIME_LIMIT}s "
          f"ants={ANTS} alpha={ALPHA} beta={BETA} rho={RHO} "
          f"ls={LOCAL_SEARCH} esaco={ESACO}{ESACO2}{ESACO3}")

    csv_path = OUT_DIR / "results.csv"
    with open(csv_path, "w", newline="") as csvf:
        w = csv.writer(csvf)
        w.writerow(["instance", "best_objective", "runtime_s", "generations"])
        for f in files:
            code = Path(f).stem
            out = OUT_DIR / f"{code}.sol"
            log = OUT_DIR / f"{code}.log"
            cmd = [BIN, f,
                   "--ants", ANTS, "--time-limit", TIME_LIMIT,
                   "--local-search", LOCAL_SEARCH,
                   "--alpha1", ALPHA, "--alpha2", BETA,
                   "--alpha3", "0", "--alpha4", "0",
                   "--eps1", "0.1", "--eps2", "0.1",
                   "--rho", RHO, "--delta", "1",
                   "--seed", SEED, "--ptries", PTRIES,
                   "--nnants", NN_ANTS, "--nnls", NN_LS,
                   "--esaco", ESACO, "--esaco2", ESACO2, "--esaco3", ESACO3,
                   "--output", str(out)]
            with open(log, "w") as lf:
                rc = subprocess.call(cmd, stdout=lf, stderr=subprocess.STDOUT)
            text = Path(log).read_text(errors="replace")
            obj = re.search(r"Best objective:\s*([0-9.]+)", text)
            rt = re.search(r"Runtime:\s*([0-9.]+)", text)
            gen = re.search(r"stopping after (\d+) generations", text)
            row = [code,
                   obj.group(1) if obj else "",
                   rt.group(1) if rt else "",
                   gen.group(1) if gen else ""]
            w.writerow(row)
            csvf.flush()
            print(f"[{code}] obj={row[1]} gen={row[3]} rt={row[2]} rc={rc}")

    print(f"done -> {csv_path}")


if __name__ == "__main__":
    main()
