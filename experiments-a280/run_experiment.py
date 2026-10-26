#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
run_experiment.py - Chay & so sanh "origin" / "topK" tren bo instances a280.

Cach dung:
    python run_experiment.py                  # chay instances x 3 seeds
    python run_experiment.py --rebuild        # compile lai ca 2 solver truoc khi chay
    python run_experiment.py --seeds 10
    python run_experiment.py --solvers origin --instances-dir PATH

Ket qua ghi vao results/ (raw_results.csv, summary.csv, head_to_head.csv).
"""

import argparse
import csv
import itertools
import math
import re
import statistics
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

# ----------------------------------------------------------------------------
# Duong dan & solver
# ----------------------------------------------------------------------------

BASE_DIR = Path(__file__).resolve().parent

SOLVERS = {
    "origin": BASE_DIR / "src-origin"   / "not-simple-2-tuneopt" / "main.exe",
    "topK":   BASE_DIR / "src-topK"     / "not-simple-2-tuneopt" / "main.exe",
}

TSP_BASE = "a280"
N_CITIES = int("".join(filter(str.isdigit, TSP_BASE)))  # 280

# ----------------------------------------------------------------------------
# Bo instances a280 theo yeu cau
# ----------------------------------------------------------------------------

ITEMS_PER_CITY = [10]
KNAPSACK_TYPES = ["bsc", "unc", "usw"]
KNAPSACK_SIZES = [10]
TRAVEL_TIMES = [3]


def build_instance_codes():
    codes = []
    for items, ktype, ksize, ttime in itertools.product(
            ITEMS_PER_CITY, KNAPSACK_TYPES, KNAPSACK_SIZES, TRAVEL_TIMES):
        codes.append((items, ktype, ksize, ttime))
    return codes


def instance_name(items, ktype, ksize, ttime):
    if ksize == "inf":
        return "%s_%02d_%s_inf_%02d" % (TSP_BASE, items, ktype, ttime)
    return "%s_%02d_%s_%02d_%02d" % (TSP_BASE, items, ktype, ksize, ttime)


# ----------------------------------------------------------------------------
# Tham so theo tung (items, knapsack_type)
# ----------------------------------------------------------------------------

parameter_configurations = {
    "a280_01_bsc": {"--ants": 200,  "--alpha1": 0.71, "--alpha2": 6.93, "--rho": 0.42, "--ptries": 2, "--local-search": 0},
    "a280_01_unc": {"--ants": 200,  "--alpha1": 1.16, "--alpha2": 1.79, "--rho": 0.42, "--ptries": 3, "--local-search": 1},
    "a280_01_usw": {"--ants": 200,  "--alpha1": 0.84, "--alpha2": 3.83, "--rho": 0.39, "--ptries": 2, "--local-search": 0},
    "a280_03_bsc": {"--ants": 100,  "--alpha1": 0.67, "--alpha2": 7.12, "--rho": 0.53, "--ptries": 1, "--local-search": 0},
    "a280_03_unc": {"--ants": 200,  "--alpha1": 0.75, "--alpha2": 4.10, "--rho": 0.74, "--ptries": 5, "--local-search": 1},
    "a280_03_usw": {"--ants": 20,   "--alpha1": 0.80, "--alpha2": 5.83, "--rho": 0.25, "--ptries": 2, "--local-search": 1},
    "a280_05_bsc": {"--ants": 1000, "--alpha1": 0.66, "--alpha2": 6.46, "--rho": 0.31, "--ptries": 1, "--local-search": 0},
    "a280_05_unc": {"--ants": 200,  "--alpha1": 0.84, "--alpha2": 5.89, "--rho": 0.24, "--ptries": 2, "--local-search": 1},
    "a280_05_usw": {"--ants": 200,  "--alpha1": 0.86, "--alpha2": 7.94, "--rho": 0.42, "--ptries": 2, "--local-search": 1},
    "a280_10_bsc": {"--ants": 200,  "--alpha1": 0.77, "--alpha2": 5.71, "--rho": 0.27, "--ptries": 2, "--local-search": 1},
    "a280_10_unc": {"--ants": 200,  "--alpha1": 0.86, "--alpha2": 6.12, "--rho": 0.30, "--ptries": 3, "--local-search": 1},
    "a280_10_usw": {"--ants": 200,  "--alpha1": 0.77, "--alpha2": 6.30, "--rho": 0.35, "--ptries": 2, "--local-search": 1},
}

# Cac tham so co dinh 
FIXED = {
    "--alpha3":        0.07,
    "--alpha4":        0.001,
    "--eps1":          0.001,
    "--eps2":          0.001,
    "--delta":         1.0,
    "--step-online":   0,
    "--delayed-online": 0,
}

random_seeds = [
    269070, 99470, 126489,
]

# ----------------------------------------------------------------------------
# Tim thu muc instances a280
# ----------------------------------------------------------------------------

WORKTREE_A280 = BASE_DIR / ".claude" / "worktrees" / "1908fix-opt" / "instances" / "a280-thop"


def resolve_instances_dir(override):
    candidates = []
    if override:
        candidates.append(Path(override))
    candidates.append(BASE_DIR / "instances" / "a280-thop")
    candidates.append(WORKTREE_A280)
    for d in candidates:
        if d.is_dir() and list(d.glob("*.thop")):
            return d
    print("[ERROR] Khong tim thay thu muc instances a280 (chua .thop nao).")
    print("        Da thu: " + ", ".join(str(c) for c in candidates))
    sys.exit(1)


# ----------------------------------------------------------------------------
# Chay 1 run
# ----------------------------------------------------------------------------

RE_PROFIT = re.compile(r"Best\s+objective:\s*([\d.eE+\-]+)", re.IGNORECASE)
RE_RUNTIME = re.compile(r"Runtime:\s*([\d.eE+\-]+)")


def time_limit(items_per_city):
    """Thoi gian chay 1 instance = tong so item / 10 (tong so item = N_CITIES * items_per_city)"""
    total_items = N_CITIES * items_per_city
    return total_items / 10.0


def build_cmd(solver, exe, input_path, cfg_key, tlimit, seed, sol_file, log_file):
    cfg = parameter_configurations[cfg_key]
    cmd = [str(exe), str(input_path)]
    for k, v in cfg.items():
        cmd += [k, str(v)]
    for k, v in FIXED.items():
        cmd += [k, str(v)]
    cmd += [
        "--time-limit", "%.1f" % tlimit,
        "--seed", str(seed),
        "--output", str(sol_file),
        "--log", str(log_file),
    ]
    return cmd


def run_one(solver, input_path, cfg_key, items, seed, tlimit, out_dir):
    exe = SOLVERS[solver]
    name = input_path.stem
    sol_file = out_dir / "solutions" / (name + ".sol")
    log_file = out_dir / "logs" / ("%s_seed%d.log" % (name, seed))
    sol_file.parent.mkdir(parents=True, exist_ok=True)
    log_file.parent.mkdir(parents=True, exist_ok=True)

    cmd = build_cmd(solver, exe, input_path, cfg_key, tlimit, seed, sol_file, log_file)

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
                "runtime": runtime, "status": "error(rc=%d) %s" % (proc.returncode, tail.strip())}

    return {"instance": name, "seed": seed, "profit": profit,
            "runtime": runtime, "status": status}


# ----------------------------------------------------------------------------
# Thong ke
# ----------------------------------------------------------------------------

def summarize(rows_by_key):
    stats = {}
    for key, rows in rows_by_key.items():
        profits = [r[0] for r in rows if r[0] is not None]
        runtimes = [r[1] for r in rows if r[1] is not None]
        stats[key] = {
            "n": len(profits),
            "mean": statistics.mean(profits) if profits else float("nan"),
            "best": max(profits) if profits else float("nan"),
            "worst": min(profits) if profits else float("nan"),
            "std": statistics.stdev(profits) if len(profits) > 1 else 0.0,
            "runtime_mean": statistics.mean(runtimes) if runtimes else float("nan"),
        }
    return stats


def head_to_head(origin_by_seed, topk_by_seed):
    seeds = sorted(set(origin_by_seed) & set(topk_by_seed))
    diffs, pcts, wins, losses, ties = [], [], 0, 0, 0
    for s in seeds:
        o, t = origin_by_seed[s], topk_by_seed[s]
        if o is None or t is None:
            continue
        d = t - o
        diffs.append(d)
        pcts.append(100.0 * d / o if o > 0 else 0.0)
        if t > o:
            wins += 1
        elif t < o:
            losses += 1
        else:
            ties += 1
    return {
        "n": len(diffs),
        "mean_diff": statistics.mean(diffs) if diffs else float("nan"),
        "mean_pct": statistics.mean(pcts) if pcts else float("nan"),
        "topk_wins": wins,
        "origin_wins": losses,
        "ties": ties,
    }


# ----------------------------------------------------------------------------
# Main
# ----------------------------------------------------------------------------

def parse_args():
    p = argparse.ArgumentParser(description="origin / topK tren a280")
    p.add_argument("--solvers", default="origin,topK",
                    help="Danh sach solver (comma-sep): origin,topK")
    p.add_argument("--instances-dir", default=None, help="Thu muc chua a280-*.thop")
    p.add_argument("--seeds", type=int, default=3, help="So seed dung (default: 3)")
    p.add_argument("--jobs", type=int, default=min(4, (__import__("os").cpu_count() or 2)),
                    help="So worker song song")
    p.add_argument("--outdir", default="results", help="Thu muc ket qua")
    p.add_argument("--rebuild", action="store_true", help="Compile lai 2 solver")
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

    solver_names = [s.strip() for s in args.solvers.split(",") if s.strip()]
    for s in solver_names:
        if s not in SOLVERS:
            print("[ERROR] Solver khong hop le: %s (co: %s)" % (s, ", ".join(SOLVERS)))
            sys.exit(1)

    if args.rebuild:
        rebuild()

    for s in solver_names:
        if not SOLVERS[s].exists():
            print("[ERROR] Khong tim thay binary: %s (chay --rebuild de compile)" % SOLVERS[s])
            sys.exit(1)

    instances_dir = resolve_instances_dir(args.instances_dir)
    n_seeds = args.seeds
    seeds = random_seeds[:n_seeds]

    codes = build_instance_codes()

    out_dir = (BASE_DIR / args.outdir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    # Danh sach task
    tasks = []
    for (items, ktype, ksize, ttime) in codes:
        name = instance_name(items, ktype, ksize, ttime)
        input_path = instances_dir / (name + ".thop")
        if not input_path.exists():
            print("[WARN] Bo qua instance thieu file: %s" % input_path)
            continue
        cfg_key = "%s_%02d_%s" % (TSP_BASE, items, ktype)
        if cfg_key not in parameter_configurations:
            print("[WARN] Khong co tham so cho %s, bo qua." % cfg_key)
            continue
        tlimit = time_limit(items)
        for solver in solver_names:
            for seed in seeds:
                tasks.append((solver, input_path, cfg_key, items, seed, tlimit, out_dir))

    total = len(tasks)
    print("=" * 72)
    print("  origin / topK  |  %s instances  |  %d seed(s)  |  tlimit = items / 10"
          % (TSP_BASE, n_seeds))
    print("  Solvers  : %s" % ", ".join(solver_names))
    print("  Instances: %d" % len(codes))
    print("  Tasks    : %d  (jobs=%d)" % (total, args.jobs))
    print("  Output   : %s/" % out_dir)
    print("=" * 72)

    results = {s: {} for s in solver_names}

    def worker(task):
        solver, input_path, cfg_key, items, seed, tlimit, out_dir = task
        r = run_one(solver, input_path, cfg_key, items, seed, tlimit, out_dir)
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

    # Ghi raw CSV
    raw_csv = out_dir / "raw_results.csv"
    with raw_csv.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["instance", "solver", "seed", "profit", "runtime", "status"])
        for r in raw_rows:
            w.writerow([r["instance"], r["solver"], r["seed"],
                        "" if r["profit"] is None else "%.6f" % r["profit"],
                        "" if r["runtime"] is None else "%.3f" % r["runtime"],
                        r["status"]])

    # Summary per (instance, solver)
    instances = sorted({r["instance"] for r in raw_rows})
    summary_csv = out_dir / "summary.csv"
    h2h_csv = out_dir / "head_to_head.csv"
    with summary_csv.open("w", newline="", encoding="utf-8") as f, \
         h2h_csv.open("w", newline="", encoding="utf-8") as g:
        sw = csv.writer(f)
        hw = csv.writer(g)
        sw.writerow(["instance", "solver", "n", "mean", "best", "worst", "std", "runtime_mean"])
        hw.writerow(["instance", "origin_mean", "topk_mean", "mean_diff", "mean_pct",
                     "topk_wins", "origin_wins", "ties"])

        print("\n" + "-" * 72)
        hdr = "%-22s" % "instance"
        for s in solver_names:
            hdr += " %10s" % s
        if "origin" in solver_names and "topK" in solver_names:
            hdr += " %12s %9s" % ("diff", "%improve")
        print(hdr)
        print("-" * 72)

        for inst in instances:
            stats = {}
            for s in solver_names:
                key = (inst, s)
                rows = [(results[s][inst][seed]["profit"],
                         results[s][inst][seed]["runtime"])
                        for seed in results[s][inst]]
                stats[s] = summarize({key: rows})[key]
                sw.writerow([inst, s, stats[s]["n"],
                            "%.6f" % stats[s]["mean"], "%.6f" % stats[s]["best"],
                            "%.6f" % stats[s]["worst"], "%.6f" % stats[s]["std"],
                            "%.3f" % stats[s]["runtime_mean"]])

            line = "%-22s" % inst
            for s in solver_names:
                line += " %10.0f" % stats[s]["mean"]

            if "origin" in stats and "topK" in stats:
                o, t = stats["origin"], stats["topK"]
                diff = t["mean"] - o["mean"]
                pct = 100.0 * diff / o["mean"] if o["mean"] > 0 else 0.0
                hh = head_to_head(
                    {sd: results["origin"][inst][sd]["profit"] for sd in results["origin"][inst]},
                    {sd: results["topK"][inst][sd]["profit"] for sd in results["topK"][inst]},
                )
                hw.writerow([inst, "%.6f" % o["mean"], "%.6f" % t["mean"],
                            "%.6f" % diff, "%.4f" % pct,
                            hh["topk_wins"], hh["origin_wins"], hh["ties"]])
                line += " %12.0f %8.2f%%  (topK %d - %d origin, %d tie)" % (
                    diff, pct, hh["topk_wins"], hh["origin_wins"], hh["ties"])
            print(line)

    print("-" * 72)
    print("Da ghi: %s, %s, %s" % (raw_csv, summary_csv, h2h_csv))


if __name__ == "__main__":
    main()