#!/usr/bin/env python3
"""Parallel seed sweep for THOP ACO (1908fix) — measure seed variance.

Runs the canonical (fixed) parameter set across many seeds in parallel,
reusing seed_sweep/results.csv (resume-safe). Reports mean/std/min/max.

Differs from seed_sweep.py only in running seeds concurrently (8 workers)
and extending the seed count.
"""
import subprocess, re, os, json, statistics
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime

ROOT = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(ROOT, "main_fresh.exe")
INSTANCE = os.path.join(ROOT, "..", "..", "instances", "dsj1000-thop", "dsj1000_10_usw_10_03.thop")
OUTDIR = os.path.join(ROOT, "seed_sweep")
os.makedirs(OUTDIR, exist_ok=True)

# Canonical parameter set (matches seed_sweep.py) — only seed varies.
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

SEEDS = list(range(1, 31))   # 30 seeds
WORKERS = 6   # 16GB RAM, ~3GB free: 6 concurrent ~150MB procs leaves headroom

results_file = os.path.join(OUTDIR, "results.csv")
config_file = os.path.join(OUTDIR, "config.json")
with open(config_file, "w") as f:
    json.dump({"instance": os.path.normpath(INSTANCE), "params": PARAMS,
               "seeds": SEEDS, "workers": WORKERS}, f, indent=2)

def run_seed(seed):
    outpath = os.path.join(OUTDIR, f"seed_{seed:02d}.out")
    logpath = os.path.join(OUTDIR, f"seed_{seed:02d}.log")
    stdoutpath = os.path.join(OUTDIR, f"seed_{seed:02d}.stdout")
    cmd = [EXE, INSTANCE]
    for k, v in PARAMS.items():
        cmd += [f"--{k}", v]
    cmd += ["--seed", str(seed), "--output", outpath]
    with open(logpath, "w") as lf, open(stdoutpath, "w") as so:
        proc = subprocess.run(cmd, stdout=so, stderr=lf, text=True)
    out = open(stdoutpath).read() if os.path.exists(stdoutpath) else ""
    obj = runtime = None
    m = re.search(r"Best objective:\s*([\d.]+)", out)
    if m: obj = float(m.group(1))
    m = re.search(r"Runtime:\s*([\d.]+)", out)
    if m: runtime = float(m.group(1))
    return seed, obj, runtime, proc.returncode

def main():
    if not os.path.exists(results_file):
        with open(results_file, "w") as f:
            f.write("seed,objective,runtime_s,exit_code\n")

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
                seed, obj, runtime, rc = fut.result()
                with open(results_file, "a") as f:
                    f.write(f"{seed},{obj if obj is not None else ''},"
                            f"{runtime if runtime is not None else ''},{rc}\n")
                print(f"[{datetime.now().strftime('%H:%M:%S')}] seed={seed:2d}  "
                      f"objective={obj}  runtime={runtime}s  rc={rc}", flush=True)

    rows = []
    for line in open(results_file):
        line = line.strip()
        if not line or line.startswith("seed"):
            continue
        p = line.split(",")
        rows.append({"seed": int(p[0]),
                     "obj": float(p[1]) if p[1] else None,
                     "t": float(p[2]) if p[2] else None})
    objs = [r["obj"] for r in rows if r["obj"] is not None]
    print("\n===== SEED SWEEP SUMMARY =====")
    print(f"instance: {os.path.basename(INSTANCE)}")
    print(f"seeds completed: {len(objs)} / {len(SEEDS)}")
    if len(objs) >= 1:
        print(f"mean    : {statistics.mean(objs):.0f}")
        if len(objs) >= 2:
            print(f"std     : {statistics.stdev(objs):.0f}  ({100*statistics.stdev(objs)/statistics.mean(objs):.3f}%)")
        print(f"median  : {statistics.median(objs):.0f}")
        print(f"min     : {min(objs):.0f}")
        print(f"max     : {max(objs):.0f}")
        print("\nper-seed (sorted desc):")
        for r in sorted(rows, key=lambda x: (x["obj"] if x["obj"] is not None else -1), reverse=True):
            print(f"  seed={r['seed']:2d}  obj={r['obj']}  t={r['t']}")

if __name__ == "__main__":
    main()
