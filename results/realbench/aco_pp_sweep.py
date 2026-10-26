#!/usr/bin/env python3
"""Parallel seed sweep for the ACO++ benchmark (benchmark/aco++).

Runs the canonical parameter set mapped from 1908fix:
  1908fix          ->  aco++
  alpha1 (pherom)  ->  --alpha
  alpha2 (heurist) ->  --beta
  rho              ->  --rho
  ants             ->  --ants
  local-search     ->  --localsearch
  ptries           ->  --ptries
  time-limit       ->  --time   (CPU seconds; ~= wall on a dedicated core)
  (delta has no aco++ equivalent)

Objective is printed as "Best solution: <profit>" (maximized profit), directly
comparable to 1908fix "Best objective". Resume-safe via aco_pp_results/results.csv.
"""
import subprocess, re, os, json, statistics
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime

ROOT = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(ROOT, "acothop.exe")
INSTANCE = os.path.join(ROOT, "..", "..", "instances", "dsj1000-thop", "dsj1000_10_usw_10_03.thop")
OUTDIR = os.path.join(ROOT, "aco_pp_results")
os.makedirs(OUTDIR, exist_ok=True)

# aco++ CLI args (order-independent), mapped from the 1908fix canonical set.
PARAMS = {
    "--mmas": None,            # MMAS variant (matches 1908fix's MMAS-style)
    "--tries": "1",
    "--ants": "50",
    "--alpha": "1.0",
    "--beta": "10.0",
    "--rho": "0.63",
    "--ptries": "20",
    "--localsearch": "3",
    "--time": "1000",
}

SEEDS = list(range(1, 31))   # match the 30 seeds used for 1908fix
WORKERS = 6

results_file = os.path.join(OUTDIR, "results.csv")
with open(os.path.join(OUTDIR, "config.json"), "w") as f:
    json.dump({"instance": os.path.normpath(INSTANCE), "params": PARAMS,
               "seeds": SEEDS, "workers": WORKERS}, f, indent=2)

def run_seed(seed):
    sol = os.path.join(OUTDIR, f"seed_{seed:02d}.sol")
    stdout = os.path.join(OUTDIR, f"seed_{seed:02d}.stdout")
    stderr = os.path.join(OUTDIR, f"seed_{seed:02d}.log")
    cmd = [EXE]
    for k, v in PARAMS.items():
        cmd.append(k)
        if v is not None:
            cmd.append(v)
    cmd += ["--seed", str(seed), "--inputfile", INSTANCE, "--outputfile", sol]
    with open(stdout, "w") as so, open(stderr, "w") as se:
        rc = subprocess.run(cmd, stdout=so, stderr=se).returncode
    text = open(stdout).read() if os.path.exists(stdout) else ""
    m = re.search(r"Best solution:\s*(\d+)", text)
    profit = int(m.group(1)) if m else None
    return seed, profit, rc

def main():
    if not os.path.exists(results_file):
        with open(results_file, "w") as f:
            f.write("seed,profit,exit_code\n")
    done = set()
    for line in open(results_file):
        line = line.strip()
        if not line or line.startswith("seed"):
            continue
        done.add(int(line.split(",")[0]))
    todo = [s for s in SEEDS if s not in done]
    print(f"{len(SEEDS)} seeds target, {len(done)} done, {len(todo)} remaining", flush=True)

    if todo:
        with ThreadPoolExecutor(max_workers=WORKERS) as ex:
            futs = {ex.submit(run_seed, s): s for s in todo}
            for fut in as_completed(futs):
                seed, profit, rc = fut.result()
                with open(results_file, "a") as f:
                    f.write(f"{seed},{profit if profit is not None else ''},{rc}\n")
                print(f"[{datetime.now().strftime('%H:%M:%S')}] seed={seed:2d}  profit={profit}  rc={rc}", flush=True)

    rows = []
    for line in open(results_file):
        line = line.strip()
        if not line or line.startswith("seed"):
            continue
        p = line.split(",")
        if p[1]:
            rows.append({"seed": int(p[0]), "profit": int(p[1])})
    profits = [r["profit"] for r in rows]
    print("\n===== ACO++ SEED SWEEP SUMMARY =====")
    print(f"instance: {os.path.basename(INSTANCE)}")
    print(f"seeds completed: {len(profits)} / {len(SEEDS)}")
    if profits:
        print(f"mean  : {statistics.mean(profits):.0f}")
        if len(profits) >= 2:
            print(f"std   : {statistics.stdev(profits):.0f}  ({100*statistics.stdev(profits)/statistics.mean(profits):.3f}%)")
        print(f"median: {statistics.median(profits):.0f}")
        print(f"min   : {min(profits)} (seed {min(rows, key=lambda r: r['profit'])['seed']})")
        print(f"max   : {max(profits)} (seed {max(rows, key=lambda r: r['profit'])['seed']})")

if __name__ == "__main__":
    main()
