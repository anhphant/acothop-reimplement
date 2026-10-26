#!/usr/bin/env python3
"""Compare two variants of the solver on the same instances & seeds:

  origin  ->  --esaco 0  --esaco2 0  --esaco3 0   (baseline aco++)
  aco     ->  --esaco 1  --esaco2 1  --esaco3 1   (ESACO: S1 + S2 + S3)

Local search is ON (2-opt) by default so S1 (pheromone-aware LS candidates) is
active. All other parameters are fixed (ACO++ sample/default). Per-instance
time limit:  time_limit = min(n_items / 10, MAX_TIME)  seconds.

Runs are parallelised across NPROC worker processes ("kernel" = CPU cores).

Env overrides:
  BIN, INSTANCES_DIR, OUT_DIR, INSTANCES (comma list of codes), FILTER,
  MAX_INSTANCES, SEEDS (comma list), MAX_TIME, NPROC,
  ANTS, ALPHA, BETA, RHO, LOCAL_SEARCH, NN_ANTS, NN_LS, PTRIES.

Example:
  docker run --rm -e NPROC=10 \
    -v "$(pwd)/instances:/app/instances" -v "$(pwd)/results:/app/results" esaco-thop
"""
import os
import re
import csv
import glob
import subprocess
import multiprocessing
from pathlib import Path


def env(name, default):
    return os.environ.get(name, default)


# --- fixed parameters (ACO++ sample/default) ---
ANTS         = env("ANTS", "10")
ALPHA        = env("ALPHA", "1.0")
BETA         = env("BETA", "2.0")
RHO          = env("RHO", "0.5")
LOCAL_SEARCH = env("LOCAL_SEARCH", "1")   # 1 = 2-opt, so ESACO S1 is active
NN_ANTS      = env("NN_ANTS", "20")
NN_LS        = env("NN_LS", "20")
PTRIES       = env("PTRIES", "1")

# --- experiment scope ---
INSTANCES_DIR = Path(env("INSTANCES_DIR", "/app/instances"))
OUT_DIR       = Path(env("OUT_DIR", "/app/results"))
BIN           = env("BIN", "/app/esaco")
SEEDS         = [int(s) for s in env("SEEDS", "10,269070,99470,126489,644764").split(",") if s.strip()]
INSTANCES     = [c.strip() for c in env("INSTANCES", "").split(",") if c.strip()]
FILTER        = env("FILTER", "")
MAX_INSTANCES = int(env("MAX_INSTANCES", "0") or "0")
MAX_TIME      = float(env("MAX_TIME", "900"))
NPROC         = int(env("NPROC", "10") or "10")


def parse_nitems(path):
    with open(path, errors="replace") as f:
        for line in f:
            if "NUMBER OF ITEMS" in line.upper():
                m = re.search(r":\s*(\d+)", line)
                if m:
                    return int(m.group(1))
    return 0


def time_limit(n_items):
    return min(n_items / 10.0, MAX_TIME)


def run_one(task):
    instance, seed, esaco = task
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    code = Path(instance).stem
    n_items = parse_nitems(instance)
    tl = time_limit(n_items)
    variant = "aco" if esaco else "origin"
    out = OUT_DIR / f"{code}_{variant}_seed{seed}.sol"
    log = OUT_DIR / f"{code}_{variant}_seed{seed}.log"
    if log.exists() and "Best objective:" in log.read_text(errors="replace"):
        return [code, seed, variant, n_items, f"{tl:.1f}", "SKIPPED", "", ""]

    cmd = [BIN, instance,
           "--ants", ANTS, "--time-limit", f"{tl:.6f}", "--local-search", LOCAL_SEARCH,
           "--alpha1", ALPHA, "--alpha2", BETA, "--alpha3", "0", "--alpha4", "0",
           "--eps1", "0.1", "--eps2", "0.1", "--rho", RHO, "--delta", "1",
           "--seed", str(seed), "--ptries", PTRIES,
           "--nnants", NN_ANTS, "--nnls", NN_LS,
           "--esaco", "1" if esaco else "0",
           "--esaco2", "1" if esaco else "0",
           "--esaco3", "1" if esaco else "0",
           "--output", str(out)]
    with open(log, "w") as lf:
        rc = subprocess.call(cmd, stdout=lf, stderr=subprocess.STDOUT)
    text = Path(log).read_text(errors="replace")
    obj = re.search(r"Best objective:\s*([0-9.]+)", text)
    gen = re.search(r"stopping after (\d+) generations", text)
    return [code, seed, variant, n_items, f"{tl:.1f}",
            (obj.group(1) if obj else ""), (gen.group(1) if gen else ""), rc]


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    if INSTANCES:
        files = []
        for code in INSTANCES:
            family = code.split("_")[0]
            p = INSTANCES_DIR / f"{family}-thop" / f"{code}.thop"
            if p.exists():
                files.append(str(p))
    else:
        files = sorted(glob.glob(str(INSTANCES_DIR / "*-thop" / "*.thop")))
        if FILTER:
            files = [f for f in files if FILTER in os.path.basename(f)]
        if MAX_INSTANCES:
            files = files[:MAX_INSTANCES]

    tasks = [(f, seed, esaco) for f in files for seed in SEEDS for esaco in (False, True)]
    print(f"instances={len(files)} seeds={SEEDS} variants=origin,aco "
          f"tasks={len(tasks)} nproc={NPROC} max_time={MAX_TIME}s "
          f"ants={ANTS} alpha={ALPHA} beta={BETA} rho={RHO} ls={LOCAL_SEARCH}")

    csv_path = OUT_DIR / "compare_results.csv"
    with open(csv_path, "w", newline="") as csvf:
        w = csv.writer(csvf)
        w.writerow(["instance", "seed", "variant", "n_items", "time_limit_s",
                    "best_objective", "generations", "rc"])
        with multiprocessing.Pool(NPROC) as pool:
            for row in pool.imap_unordered(run_one, tasks, chunksize=1):
                w.writerow(row)
                csvf.flush()
                print(f"[{row[0]} seed={row[1]} {row[2]:6s}] tl={row[4]}s "
                      f"obj={row[5]} gen={row[6]} rc={row[7]}")

    print(f"done -> {csv_path}")


if __name__ == "__main__":
    main()
