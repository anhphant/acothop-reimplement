#!/usr/bin/env python3
"""Seed sweep for THOP ACO solver (1908fix). Vary seed, fix params, aggregate."""
import subprocess, re, os, sys, json, statistics
from datetime import datetime

ROOT = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(ROOT, "main_fresh.exe")
INSTANCE = os.path.join(ROOT, "..", "..", "instances", "dsj1000-thop", "dsj1000_10_usw_10_03.thop")
OUTDIR = os.path.join(ROOT, "seed_sweep")
os.makedirs(OUTDIR, exist_ok=True)

# Fixed canonical parameter set (run.ps1 defaults; only seed varies)
PARAMS = {
    "ants": "50",
    "time-limit": "1000",
    "local-search": "3",
    "alpha1": "1.0",
    "alpha2": "10.0",
    "alpha3": "0.0",
    "alpha4": "0.0",
    "rho": "0.63",
    "delta": "1.0",
    "ptries": "20",
    "step-online": "0",
    "delayed-online": "0",
    "adaptive": "0",
}

SEEDS = list(range(1, 21))  # 20 seeds

results_file = os.path.join(OUTDIR, "results.csv")
config_file = os.path.join(OUTDIR, "config.json")
with open(config_file, "w") as f:
    json.dump({"instance": os.path.normpath(INSTANCE), "params": PARAMS, "seeds": SEEDS}, f, indent=2)

def run_seed(seed):
    outpath = os.path.join(OUTDIR, f"seed_{seed:02d}.out")
    logpath = os.path.join(OUTDIR, f"seed_{seed:02d}.log")
    cmd = [EXE, INSTANCE]
    for k, v in PARAMS.items():
        cmd += [f"--{k}", v]
    cmd += ["--seed", str(seed), "--output", outpath]
    with open(logpath, "w") as lf:
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=lf, text=True)
    out = proc.stdout or ""
    obj = None
    m = re.search(r"Best objective:\s*([\d.]+)", out)
    if m:
        obj = float(m.group(1))
    runtime = None
    m = re.search(r"Runtime:\s*([\d.]+)", out)
    if m:
        runtime = float(m.group(1))
    return seed, obj, runtime, proc.returncode

# Append header if new
if not os.path.exists(results_file):
    with open(results_file, "w") as f:
        f.write("seed,objective,runtime_s,exit_code\n")

# Resume: skip seeds already completed (persisted in results.csv)
done = set()
with open(results_file) as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith("seed"):
            continue
        done.add(int(line.split(",")[0]))
todo = [s for s in SEEDS if s not in done]
if todo:
    print(f"resuming: {len(done)} seed(s) already done, {len(todo)} remaining: {todo}", flush=True)
else:
    print("all seeds already done — printing summary from existing results", flush=True)

summary = []
for s in todo:
    seed, obj, runtime, rc = run_seed(s)
    summary.append({"seed": seed, "objective": obj, "runtime": runtime, "exit_code": rc})
    with open(results_file, "a") as f:
        f.write(f"{seed},{obj if obj is not None else ''},{runtime if runtime is not None else ''},{rc}\n")
    print(f"[{datetime.now().strftime('%H:%M:%S')}] seed={seed:2d}  objective={obj}  runtime={runtime}s  rc={rc}", flush=True)

# Aggregate over ALL persisted results (resume-safe), not just this run
all_rows = []
with open(results_file) as f:
    for line in f:
        line = line.strip()
        if not line or line.startswith("seed"):
            continue
        parts = line.split(",")
        all_rows.append({
            "seed": int(parts[0]),
            "objective": float(parts[1]) if parts[1] else None,
            "runtime": float(parts[2]) if parts[2] else None,
        })

objs = [r["objective"] for r in all_rows if r["objective"] is not None]
print("\n===== SEED SWEEP SUMMARY =====")
print(f"instance: {os.path.basename(INSTANCE)}")
print(f"params: {PARAMS}")
print(f"seeds completed: {len(all_rows)} / {len(SEEDS)}")
if objs:
    print(f"objective mean   : {statistics.mean(objs):.0f}")
    print(f"objective std    : {statistics.stdev(objs):.0f}")
    print(f"objective median : {statistics.median(objs):.0f}")
    print(f"objective min    : {min(objs):.0f}")
    print(f"objective max    : {max(objs):.0f}")
    best = max(all_rows, key=lambda r: (r["objective"] if r["objective"] is not None else -1))
    print(f"BEST seed        : {best['seed']}  ->  {best['objective']:.0f}")
    print("\nper-seed (sorted desc):")
    for r in sorted(all_rows, key=lambda r: (r["objective"] if r["objective"] is not None else -1), reverse=True):
        print(f"  seed={r['seed']:2d}  objective={r['objective']}  runtime={r['runtime']}s")
else:
    print("NO valid objective values parsed — check logs.")
