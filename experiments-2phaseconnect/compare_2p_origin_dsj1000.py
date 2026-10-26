#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
compare_2p_origin_dsj1000.py - So sanh src-2p (item pheromone) vs src-origin tren
TOAN BO bo instance dsj1000 (tat ca *.thop trong instances/dsj1000-thop).

    python3 compare_2p_origin_dsj1000.py --rebuild --jobs 8     # toan bo instance x 3 seeds
    python3 compare_2p_origin_dsj1000.py --seeds 10
    python3 compare_2p_origin_dsj1000.py --rho-item 0.1 --alpha-item 0.25

Bo tham so ACO (ants/alpha/beta/rho/ptries/localsearch) lay theo tung (items,
knapsack_type) tu run_aco++_experiments.py (parameter_configurations). Cac flag
alpha3/alpha4/eps1/eps2/delta/... khong truyen -> solver dung gia tri mac dinh.
rho-item / alpha-item chi co o src-2p, duoc fine-tune rieng truoc khi chay.
"""

import argparse
import csv
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

from run_experiment import summarize, head_to_head, RE_PROFIT, RE_RUNTIME

BASE_DIR = Path(__file__).resolve().parent

SOLVERS = {
    "2p":     BASE_DIR / "src-2p"     / "not-simple-2-tuneopt" / "main.exe",
    "origin": BASE_DIR / "src-origin" / "not-simple-2-tuneopt" / "main.exe",
}

TSP_BASE = "dsj1000"
N_CITIES = int("".join(filter(str.isdigit, TSP_BASE)))  # 1000

# Bo tham so ACO theo tung (items, knapsack_type), lay tu run_aco++_experiments.py.
parameter_configurations = {
    "dsj1000_01_bsc": {"--ants": 200, "--alpha": 4.30, "--beta": 9.18, "--rho": 0.27, "--ptries": 4, "--localsearch": 1},
    "dsj1000_01_unc": {"--ants": 50,  "--alpha": 8.33, "--beta": 6.39, "--rho": 0.10, "--ptries": 5, "--localsearch": 1},
    "dsj1000_01_usw": {"--ants": 100, "--alpha": 2.49, "--beta": 6.19, "--rho": 0.37, "--ptries": 5, "--localsearch": 1},
    "dsj1000_03_bsc": {"--ants": 50,  "--alpha": 0.99, "--beta": 5.16, "--rho": 0.81, "--ptries": 4, "--localsearch": 1},
    "dsj1000_03_unc": {"--ants": 100, "--alpha": 2.71, "--beta": 8.81, "--rho": 0.25, "--ptries": 2, "--localsearch": 1},
    "dsj1000_03_usw": {"--ants": 100, "--alpha": 0.90, "--beta": 5.03, "--rho": 0.84, "--ptries": 5, "--localsearch": 1},
    "dsj1000_05_bsc": {"--ants": 20,  "--alpha": 0.88, "--beta": 6.10, "--rho": 0.69, "--ptries": 3, "--localsearch": 1},
    "dsj1000_05_unc": {"--ants": 100, "--alpha": 5.93, "--beta": 8.50, "--rho": 0.12, "--ptries": 5, "--localsearch": 1},
    "dsj1000_05_usw": {"--ants": 100, "--alpha": 0.88, "--beta": 6.18, "--rho": 0.83, "--ptries": 2, "--localsearch": 1},
    "dsj1000_10_bsc": {"--ants": 500, "--alpha": 3.20, "--beta": 7.89, "--rho": 0.21, "--ptries": 1, "--localsearch": 1},
    "dsj1000_10_unc": {"--ants": 500, "--alpha": 2.94, "--beta": 7.96, "--rho": 0.26, "--ptries": 1, "--localsearch": 1},
    "dsj1000_10_usw": {"--ants": 500, "--alpha": 4.04, "--beta": 7.82, "--rho": 0.39, "--ptries": 2, "--localsearch": 1},
}


def cfg_key_of(items, ktype):
    return "%s_%02d_%s" % (TSP_BASE, items, ktype)

random_seeds = [
    269070, 99470, 126489,
]

INSTANCES_DIR = BASE_DIR / "instances" / "dsj1000-thop"


def resolve_instances_dir(override):
    if override:
        d = Path(override)
        if d.is_dir() and list(d.glob("*.thop")):
            return d
    if INSTANCES_DIR.is_dir() and list(INSTANCES_DIR.glob("*.thop")):
        return INSTANCES_DIR
    print("[ERROR] Khong tim thay thu muc instances dsj1000 (chua .thop nao).")
    print("        Da thu: %s" % INSTANCES_DIR)
    sys.exit(1)


def discover_instances(instances_dir):
    """Tra ve (items, ktype, ksize, ttime, path) cho moi file *.thop."""
    found = []
    for p in sorted(instances_dir.glob("*.thop")):
        parts = p.stem.split("_")  # dsj1000_01_bsc_01_01 | dsj1000_01_bsc_inf_01
        if len(parts) != 5 or parts[0] != TSP_BASE:
            continue
        items = int(parts[1])
        ktype = parts[2]
        ksize = parts[3]   # "01"/"05"/"10"/"inf"
        ttime = int(parts[4])
        found.append((items, ktype, ksize, ttime, p))
    return found


def time_limit(items_per_city):
    """Thoi gian chay 1 instance = tong so item / 10 (tong so item = N_CITIES * items)."""
    return N_CITIES * items_per_city / 10.0


def build_cmd(solver, exe, input_path, cfg_key, tlimit, seed, sol_file, log_file,
              rho_item, alpha_item):
    cfg = parameter_configurations[cfg_key]
    cmd = [str(exe), str(input_path)]
    for k, v in cfg.items():
        cmd += [k, str(v)]
    # Item pheromone chi co o src-2p (src-origin khong co flag nay).
    if solver == "2p":
        cmd += ["--rho-item", str(rho_item), "--alpha-item", str(alpha_item)]
    cmd += [
        "--time-limit", "%.1f" % tlimit,
        "--seed", str(seed),
        "--output", str(sol_file),
        "--log", str(log_file),
    ]
    return cmd


def run_one(solver, input_path, cfg_key, seed, tlimit, out_dir, rho_item, alpha_item):
    exe = SOLVERS[solver]
    name = input_path.stem
    sol_file = out_dir / "solutions" / ("%s_%s.sol" % (name, solver))
    log_file = out_dir / "logs" / ("%s_%s_seed%d.log" % (name, solver, seed))
    sol_file.parent.mkdir(parents=True, exist_ok=True)
    log_file.parent.mkdir(parents=True, exist_ok=True)

    cmd = build_cmd(solver, exe, input_path, cfg_key, tlimit, seed,
                    sol_file, log_file, rho_item, alpha_item)

    try:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              timeout=tlimit + 60, cwd=exe.parent)
    except subprocess.TimeoutExpired:
        return {"instance": name, "seed": seed, "profit": None, "runtime": None,
                "status": "timeout"}
    except OSError as e:
        return {"instance": name, "seed": seed, "profit": None, "runtime": None,
                "status": "oserror:%s" % e}

    out = proc.stdout + "\n" + proc.stderr
    m = RE_PROFIT.search(out)
    rt = RE_RUNTIME.search(out)
    profit = float(m.group(1)) if m else None
    runtime = float(rt.group(1)) if rt else None
    status = "ok" if (proc.returncode == 0 and profit is not None) else "error"

    if status != "ok":
        tail = ""
        try:
            tail = "\n".join(log_file.read_text(errors="replace").splitlines()[-3:])
        except OSError:
            pass
        if not tail.strip():
            tail = "\n".join(proc.stderr.splitlines()[-3:])
        return {"instance": name, "seed": seed, "profit": profit,
                "runtime": runtime,
                "status": "error(rc=%d) %s" % (proc.returncode, tail.strip())}

    return {"instance": name, "seed": seed, "profit": profit,
            "runtime": runtime, "status": status}


def parse_args():
    p = argparse.ArgumentParser(description="src-2p vs src-origin tren toan bo dsj1000")
    p.add_argument("--instances-dir", default=None)
    p.add_argument("--seeds", type=int, default=3)
    p.add_argument("--jobs", type=int, default=8)
    p.add_argument("--outdir", default="results_2p_origin_dsj1000")
    p.add_argument("--rho-item", type=float, default=0.1)
    p.add_argument("--alpha-item", type=float, default=0.25)
    p.add_argument("--rebuild", action="store_true")
    return p.parse_args()


def rebuild():
    for label, exe in SOLVERS.items():
        src = exe.parent / "main.cpp"
        print("[rebuild] %s: g++ -std=c++17 -O2 %s -o %s" % (label, src.name, exe.name))
        r = subprocess.run(["g++", "-std=c++17", "-O2", str(src), "-o", str(exe)],
                           cwd=exe.parent)
        if r.returncode != 0:
            print("[ERROR] Compile %s that bai." % label)
            sys.exit(1)


def main():
    args = parse_args()

    if args.rebuild:
        rebuild()

    for label, exe in SOLVERS.items():
        if not exe.exists():
            print("[ERROR] Khong tim thay binary: %s (chay --rebuild de compile)" % exe)
            sys.exit(1)

    instances_dir = resolve_instances_dir(args.instances_dir)
    seeds = random_seeds[:args.seeds]

    out_dir = (BASE_DIR / args.outdir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    found = discover_instances(instances_dir)

    tasks = []
    for (items, ktype, ksize, ttime, input_path) in found:
        cfg_key = cfg_key_of(items, ktype)
        if cfg_key not in parameter_configurations:
            print("[WARN] Khong co tham so cho %s, bo qua." % cfg_key)
            continue
        tlimit = time_limit(items)
        for solver in SOLVERS:
            for seed in seeds:
                tasks.append((solver, input_path, cfg_key, seed, tlimit, out_dir))

    total = len(tasks)
    print("=" * 72)
    print("  src-2p / src-origin  |  %s  |  %d instance(s)  |  %d seed(s)  |  jobs=%d"
          % (TSP_BASE, len(found), args.seeds, args.jobs))
    print("  rho-item=%.4g alpha-item=%.4g" % (args.rho_item, args.alpha_item))
    print("  Tasks: %d   Output: %s/" % (total, out_dir))
    print("=" * 72)

    results = {s: {} for s in SOLVERS}

    def worker(task):
        solver, input_path, cfg_key, seed, tlimit, out_dir = task
        r = run_one(solver, input_path, cfg_key, seed, tlimit, out_dir,
                    args.rho_item, args.alpha_item)
        r["solver"] = solver
        return r

    raw_rows = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futs = [pool.submit(worker, t) for t in tasks]
        for fut in as_completed(futs):
            r = fut.result()
            raw_rows.append(r)
            results[r["solver"]].setdefault(r["instance"], {})[r["seed"]] = r
            if r["status"] == "ok":
                print("[%s] %s seed=%d profit=%.0f runtime=%.1fs"
                      % (r["solver"], r["instance"], r["seed"], r["profit"], r["runtime"]),
                      flush=True)
            else:
                print("[%s] %s seed=%d %s" % (r["solver"], r["instance"], r["seed"], r["status"]),
                      flush=True)

    raw_csv = out_dir / "raw_results.csv"
    with raw_csv.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["instance", "solver", "seed", "profit", "runtime", "status"])
        for r in raw_rows:
            w.writerow([r["instance"], r["solver"], r["seed"],
                        "" if r["profit"] is None else "%.6f" % r["profit"],
                        "" if r["runtime"] is None else "%.3f" % r["runtime"],
                        r["status"]])

    instances = sorted({r["instance"] for r in raw_rows})
    summary_csv = out_dir / "summary.csv"
    h2h_csv = out_dir / "head_to_head.csv"
    with summary_csv.open("w", newline="", encoding="utf-8") as f, \
         h2h_csv.open("w", newline="", encoding="utf-8") as g:
        sw = csv.writer(f)
        hw = csv.writer(g)
        sw.writerow(["instance", "solver", "n", "mean", "best", "worst", "std", "runtime_mean"])
        hw.writerow(["instance", "origin_mean", "2p_mean", "mean_diff", "mean_pct",
                     "2p_wins", "origin_wins", "ties"])

        print("\n" + "-" * 72)
        print("%-24s %12s %12s %12s %9s" % ("instance", "origin", "2p", "diff", "%improve"))
        print("-" * 72)

        for inst in instances:
            stats = {}
            for s in SOLVERS:
                rows = [(results[s][inst][sd]["profit"],
                         results[s][inst][sd]["runtime"])
                        for sd in results[s][inst]]
                key = (inst, s)
                stats[s] = summarize({key: rows})[key]
                sw.writerow([inst, s, stats[s]["n"],
                             "%.6f" % stats[s]["mean"], "%.6f" % stats[s]["best"],
                             "%.6f" % stats[s]["worst"], "%.6f" % stats[s]["std"],
                             "%.3f" % stats[s]["runtime_mean"]])

            o, t = stats["origin"], stats["2p"]
            diff = t["mean"] - o["mean"]
            pct = 100.0 * diff / o["mean"] if o["mean"] > 0 else 0.0
            hh = head_to_head(
                {sd: results["origin"][inst][sd]["profit"] for sd in results["origin"][inst]},
                {sd: results["2p"][inst][sd]["profit"] for sd in results["2p"][inst]},
            )
            hw.writerow([inst, "%.6f" % o["mean"], "%.6f" % t["mean"],
                         "%.6f" % diff, "%.4f" % pct,
                         hh["topk_wins"], hh["origin_wins"], hh["ties"]])
            print("%-24s %12.0f %12.0f %12.0f %8.2f%%  (2p %d - %d origin, %d tie)"
                  % (inst, o["mean"], t["mean"], diff, pct,
                     hh["topk_wins"], hh["origin_wins"], hh["ties"]))

    print("-" * 72)
    print("Da ghi: %s, %s, %s" % (raw_csv, summary_csv, h2h_csv))


if __name__ == "__main__":
    main()
