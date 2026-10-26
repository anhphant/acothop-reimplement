#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
compare_2p_origin.py - So sanh src-2p (item pheromone) vs src-origin tren a280.

    python compare_2p_origin.py                # 3 instances x 3 seeds x 2 solvers
    python compare_2p_origin.py --seeds 10
    python compare_2p_origin.py --rebuild
    python compare_2p_origin.py --rho-item 0.3 --alpha-item 0.0625

Tham so (ants/alpha/rho/...) lay tu run_experiment.py; them item pheromone cho src-2p.
"""

import argparse
import csv
import statistics
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

import run_experiment as RX

BASE_DIR = Path(__file__).resolve().parent

SOLVERS = {
    "2p":     BASE_DIR / "src-2p"     / "not-simple-2-tuneopt" / "main.exe",
    "origin": BASE_DIR / "src-origin" / "not-simple-2-tuneopt" / "main.exe",
}


def build_cmd(solver, exe, input_path, cfg_key, tlimit, seed, sol_file, log_file,
              rho_item, alpha_item):
    cfg = RX.parameter_configurations[cfg_key]
    cmd = [str(exe), str(input_path)]
    for k, v in cfg.items():
        cmd += [k, str(v)]
    for k, v in RX.FIXED.items():
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


def run_one(solver, input_path, cfg_key, items, seed, tlimit, out_dir,
            rho_item, alpha_item):
    exe = SOLVERS[solver]
    name = input_path.stem
    sol_file = out_dir / "solutions" / (name + ".sol")
    log_file = out_dir / "logs" / ("%s_seed%d.log" % (name, seed))
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
    m = RX.RE_PROFIT.search(out)
    rt = RX.RE_RUNTIME.search(out)
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
    p = argparse.ArgumentParser(description="src-2p vs src-origin tren a280")
    p.add_argument("--instances-dir", default=None)
    p.add_argument("--seeds", type=int, default=3)
    p.add_argument("--jobs", type=int, default=8)
    p.add_argument("--outdir", default="results_2p_origin")
    p.add_argument("--rho-item", type=float, default=0.3)
    p.add_argument("--alpha-item", type=float, default=0.0625)
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

    instances_dir = RX.resolve_instances_dir(args.instances_dir)
    seeds = RX.random_seeds[:args.seeds]

    out_dir = (BASE_DIR / args.outdir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    # Chi chay bo a280_10_xxx_10_03
    CODES = [(10, "bsc", 10, 3), (10, "unc", 10, 3), (10, "usw", 10, 3)]

    tasks = []
    for (items, ktype, ksize, ttime) in CODES:
        name = RX.instance_name(items, ktype, ksize, ttime)
        input_path = instances_dir / (name + ".thop")
        if not input_path.exists():
            print("[WARN] Bo qua instance thieu file: %s" % input_path)
            continue
        cfg_key = "%s_%02d_%s" % (RX.TSP_BASE, items, ktype)
        if cfg_key not in RX.parameter_configurations:
            print("[WARN] Khong co tham so cho %s, bo qua." % cfg_key)
            continue
        tlimit = RX.time_limit(items)
        for solver in SOLVERS:
            for seed in seeds:
                tasks.append((solver, input_path, cfg_key, items, seed, tlimit, out_dir))

    total = len(tasks)
    print("=" * 72)
    print("  src-2p / src-origin  |  %s  |  %d seed(s)  |  rho-item=%.4g alpha-item=%.4g"
          % (RX.TSP_BASE, args.seeds, args.rho_item, args.alpha_item))
    print("  Instances: %d   Tasks: %d  (jobs=%d)" % (len(tasks) // (len(SOLVERS) * args.seeds), total, args.jobs))
    print("  Output   : %s/" % out_dir)
    print("=" * 72)

    results = {s: {} for s in SOLVERS}

    def worker(task):
        solver, input_path, cfg_key, items, seed, tlimit, out_dir = task
        r = run_one(solver, input_path, cfg_key, items, seed, tlimit, out_dir,
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
        print("%-22s %12s %12s %12s %9s" % ("instance", "origin", "2p", "diff", "%improve"))
        print("-" * 72)

        for inst in instances:
            stats = {}
            for s in SOLVERS:
                rows = [(results[s][inst][sd]["profit"],
                         results[s][inst][sd]["runtime"])
                        for sd in results[s][inst]]
                key = (inst, s)
                stats[s] = RX.summarize({key: rows})[key]
                sw.writerow([inst, s, stats[s]["n"],
                             "%.6f" % stats[s]["mean"], "%.6f" % stats[s]["best"],
                             "%.6f" % stats[s]["worst"], "%.6f" % stats[s]["std"],
                             "%.3f" % stats[s]["runtime_mean"]])

            o, t = stats["origin"], stats["2p"]
            diff = t["mean"] - o["mean"]
            pct = 100.0 * diff / o["mean"] if o["mean"] > 0 else 0.0
            hh = RX.head_to_head(
                {sd: results["origin"][inst][sd]["profit"] for sd in results["origin"][inst]},
                {sd: results["2p"][inst][sd]["profit"] for sd in results["2p"][inst]},
            )
            hw.writerow([inst, "%.6f" % o["mean"], "%.6f" % t["mean"],
                         "%.6f" % diff, "%.4f" % pct,
                         hh["topk_wins"], hh["origin_wins"], hh["ties"]])
            print("%-22s %12.0f %12.0f %12.0f %8.2f%%  (2p %d - %d origin, %d tie)"
                  % (inst, o["mean"], t["mean"], diff, pct,
                     hh["topk_wins"], hh["origin_wins"], hh["ties"]))

    print("-" * 72)
    print("Da ghi: %s, %s, %s" % (raw_csv, summary_csv, h2h_csv))


if __name__ == "__main__":
    main()
