#!/usr/bin/env python3
"""Parameter tuning for THOP ACO (1908fix).

Screens the parameters that actually affect the solver:
  alpha1 (pheromone exponent), alpha2 (heuristic exponent),
  rho (evaporation), delta (deposit), ants, local-search, ptries.

alpha3/alpha4/adaptive are parsed but unused in this codebase, so they are
kept fixed. Runs configs in parallel (default 6 workers) and is resume-safe:
completed configs are skipped on re-run.

Output: tuning/results.csv + per-config stdout/stderr under tuning/.
"""
import subprocess, re, os, sys, json, itertools, statistics
from concurrent.futures import ThreadPoolExecutor, as_completed

ROOT = os.path.dirname(os.path.abspath(__file__))
EXE = os.path.join(ROOT, "main_fresh.exe")
INSTANCE = os.path.join(ROOT, "..", "..", "instances", "dsj1000-thop", "dsj1000_10_usw_10_03.thop")
OUTDIR = os.path.join(ROOT, "tuning")
os.makedirs(OUTDIR, exist_ok=True)

# ---- fixed defaults (canonical set from seed_sweep) ----
BASE = {
    "ants": "50",
    "time_limit": "100",       # screening budget (s)
    "local_search": "3",
    "alpha1": "1.0",
    "alpha2": "10.0",
    "alpha3": "0.0",           # unused
    "alpha4": "0.0",           # unused
    "rho": "0.63",
    "delta": "1.0",
    "ptries": "20",
    "step_online": "0",
    "delayed_online": "0",
    "adaptive": "0",           # unused
    "seed": "1910",
}

# ---- grid to sweep (cartesian product) ----
GRID = {
    "alpha2": ["5", "7", "10", "15"],
    "alpha1": ["0.5", "1.0", "1.5"],
    "rho":    ["0.4", "0.63", "0.8"],
}

WORKERS = 6

RESULTS = os.path.join(OUTDIR, "results.csv")
CONFIG_FILE = os.path.join(OUTDIR, "config.json")

def configs():
    keys = list(GRID.keys())
    out = []
    for vals in itertools.product(*(GRID[k] for k in keys)):
        cfg = dict(BASE)
        for k, v in zip(keys, vals):
            cfg[k] = v
        out.append(cfg)
    return out

def cfg_id(cfg):
    return "_".join(f"{k}={cfg[k]}" for k in sorted(GRID.keys()))

def run_one(cfg):
    cid = cfg_id(cfg)
    outfile = os.path.join(OUTDIR, f"cfg_{cid}.out")
    stdout = os.path.join(OUTDIR, f"cfg_{cid}.stdout")
    stderr = os.path.join(OUTDIR, f"cfg_{cid}.log")

    cmd = [EXE, INSTANCE]
    for k in sorted(cfg.keys()):
        cmd += [f"--{k.replace('_', '-')}", cfg[k]]
    cmd += ["--output", outfile]

    with open(stdout, "w") as so, open(stderr, "w") as se:
        rc = subprocess.run(cmd, stdout=so, stderr=se).returncode

    text = open(stdout).read() if os.path.exists(stdout) else ""
    obj = runtime = None
    m = re.search(r"Best objective:\s*([\d.]+)", text)
    if m: obj = float(m.group(1))
    m = re.search(r"Runtime:\s*([\d.]+)", text)
    if m: runtime = float(m.group(1))
    return cid, cfg, obj, runtime, rc

def main():
    cfgs = configs()
    with open(CONFIG_FILE, "w") as f:
        json.dump({"instance": os.path.normpath(INSTANCE), "grid": GRID,
                   "base": BASE, "n_configs": len(cfgs)}, f, indent=2)

    # resume: load done ids
    done = set()
    if os.path.exists(RESULTS):
        for line in open(RESULTS):
            line = line.strip()
            if not line or line.startswith("cfg"):
                continue
            done.add(line.split(",")[0])

    todo = [c for c in cfgs if cfg_id(c) not in done]
    print(f"{len(cfgs)} configs total, {len(done)} done, {len(todo)} to run", flush=True)

    if todo:
        if not os.path.exists(RESULTS):
            with open(RESULTS, "w") as f:
                f.write("cfg,alpha1,alpha2,rho,objective,runtime_s,exit_code\n")
        with ThreadPoolExecutor(max_workers=WORKERS) as ex:
            futs = {ex.submit(run_one, c): c for c in todo}
            for fut in as_completed(futs):
                cid, cfg, obj, runtime, rc = fut.result()
                with open(RESULTS, "a") as f:
                    f.write(f"{cid},{cfg['alpha1']},{cfg['alpha2']},{cfg['rho']},"
                            f"{obj if obj is not None else ''},"
                            f"{runtime if runtime is not None else ''},{rc}\n")
                print(f"[done] {cid}  obj={obj}  t={runtime}s  rc={rc}", flush=True)

    # aggregate + rank
    rows = []
    for line in open(RESULTS):
        line = line.strip()
        if not line or line.startswith("cfg"):
            continue
        p = line.split(",")
        rows.append({"cfg": p[0], "a1": p[1], "a2": p[2], "rho": p[3],
                     "obj": float(p[4]) if p[4] else None,
                     "t": float(p[5]) if p[5] else None})
    valid = [r for r in rows if r["obj"] is not None]
    print("\n===== TUNING SUMMARY (ranked by objective desc) =====")
    for i, r in enumerate(sorted(valid, key=lambda x: x["obj"], reverse=True)):
        print(f"  {i+1:2d}. a1={r['a1']:>4}  a2={r['a2']:>4}  rho={r['rho']:>5}  "
              f"obj={r['obj']:.0f}  t={r['t']}")
    if valid:
        best = max(valid, key=lambda x: x["obj"])
        print(f"\nBEST: alpha1={best['a1']} alpha2={best['a2']} rho={best['rho']} -> {best['obj']:.0f}")

if __name__ == "__main__":
    main()
