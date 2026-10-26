#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
compare_initant.py - So sanh initAnt (gen 1 dung init_ants ant) vs baseline
(gen 1 dung number_of_ants) tren bo a280, dung CUNG 1 binary
original-2phaseconnect-initACO (co flag --init-ants).

    python compare_initant.py --init-ants 1000
    python compare_initant.py --init-ants 1000 --seeds 10 --rebuild
    python compare_initant.py --init-ants 1000 --rho-item 0.3 --alpha-item 0.0625

Hai phia chi khac nhau o --init-ants:
    base    = --init-ants 0  (gen 1 cung so ant nhu cac gen sau)
    initant = --init-ants N  (gen 1 dung N ant)
"""

import argparse
import csv
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

import run_experiment as RX

BASE_DIR = Path(__file__).resolve().parent

# Cung 1 binary cho ca 2 phia; init_ants == 0 -> baseline.
INITANT_EXE = (BASE_DIR.parent / "original-2phaseconnect-initACO"
               / "src" / "not-simple-2-tuneopt" / "main.exe")

TAGS = ["base", "initant"]


def build_cmd(exe, input_path, cfg_key, tlimit, seed, sol_file, log_file,
              init_ants, rho_item, alpha_item):
    cfg = RX.parameter_configurations[cfg_key]
    cmd = [str(exe), str(input_path)]
    for k, v in cfg.items():
        cmd += [k, str(v)]
    for k, v in RX.FIXED.items():
        cmd += [k, str(v)]
    # 2-phase (item pheromone) giong compare_2p_origin.py; ca 2 phia giong nhau.
    cmd += ["--rho-item", str(rho_item), "--alpha-item", str(alpha_item)]
    cmd += [
        "--init-ants", str(init_ants),
        "--time-limit", "%.1f" % tlimit,
        "--seed", str(seed),
        "--output", str(sol_file),
        "--log", str(log_file),
    ]
    return cmd


def run_one(init_ants, input_path, cfg_key, items, seed, tlimit, out_dir,
            rho_item, alpha_item):
    exe = INITANT_EXE
    name = input_path.stem
    tag = "initant" if init_ants > 0 else "base"
    sol_file = out_dir / "solutions" / ("%s_%s.sol" % (name, tag))
    log_file = out_dir / "logs" / ("%s_%s_seed%d.log" % (name, tag, seed))
    sol_file.parent.mkdir(parents=True, exist_ok=True)
    log_file.parent.mkdir(parents=True, exist_ok=True)

    cmd = build_cmd(exe, input_path, cfg_key, tlimit, seed,
                    sol_file, log_file, init_ants, rho_item, alpha_item)

    try:
        proc = subprocess.run(cmd, capture_output=True, text=True,
                              timeout=tlimit + 60, cwd=exe.parent)
    except subprocess.TimeoutExpired:
        return {"instance": name, "seed": seed, "tag": tag,
                "profit": None, "runtime": None, "status": "timeout"}
    except OSError as e:
        return {"instance": name, "seed": seed, "tag": tag,
                "profit": None, "runtime": None, "status": "oserror:%s" % e}

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
        return {"instance": name, "seed": seed, "tag": tag, "profit": profit,
                "runtime": runtime, "status": "error(rc=%d) %s" % (proc.returncode, tail.strip())}

    return {"instance": name, "seed": seed, "tag": tag,
            "profit": profit, "runtime": runtime, "status": status}


def parse_args():
    p = argparse.ArgumentParser(description="initAnt vs baseline tren a280")
    p.add_argument("--init-ants", type=int, required=True,
                   help="So ant dung o generation 1 (phia initant)")
    p.add_argument("--instances-dir", default=None)
    p.add_argument("--seeds", type=int, default=3)
    p.add_argument("--jobs", type=int, default=8)
    p.add_argument("--outdir", default="results_initant")
    p.add_argument("--rho-item", type=float, default=0.3)
    p.add_argument("--alpha-item", type=float, default=0.0625)
    p.add_argument("--rebuild", action="store_true")
    return p.parse_args()


def rebuild():
    exe = INITANT_EXE
    src = exe.parent / "main.cpp"
    print("[rebuild] initant: g++ -std=c++17 -O2 %s -o %s" % (src.name, exe.name))
    r = subprocess.run(["g++", "-std=c++17", "-O2", str(src), "-o", str(exe)],
                       cwd=exe.parent)
    if r.returncode != 0:
        print("[ERROR] Compile initant that bai.")
        sys.exit(1)


def main():
    args = parse_args()

    if args.rebuild:
        rebuild()

    if not INITANT_EXE.exists():
        print("[ERROR] Khong tim thay binary: %s (chay --rebuild de compile)" % INITANT_EXE)
        sys.exit(1)

    instances_dir = RX.resolve_instances_dir(args.instances_dir)
    seeds = RX.random_seeds[:args.seeds]

    out_dir = (BASE_DIR / args.outdir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

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
        # base (init_ants=0) va initant (init_ants=N)
        for init_ants in (0, args.init_ants):
            for seed in seeds:
                tasks.append((init_ants, input_path, cfg_key, items, seed, tlimit, out_dir))

    total = len(tasks)
    print("=" * 72)
    print("  initAnt / baseline  |  %s  |  %d seed(s)  |  init-ants=%d"
          % (RX.TSP_BASE, args.seeds, args.init_ants))
    print("  rho-item=%.4g alpha-item=%.4g" % (args.rho_item, args.alpha_item))
    print("  Instances: %d   Tasks: %d  (jobs=%d)" % (len(CODES), total, args.jobs))
    print("  Output   : %s/" % out_dir)
    print("=" * 72)

    results = {tag: {} for tag in TAGS}

    def worker(task):
        init_ants, input_path, cfg_key, items, seed, tlimit, out_dir = task
        r = run_one(init_ants, input_path, cfg_key, items, seed, tlimit, out_dir,
                    args.rho_item, args.alpha_item)
        return r

    raw_rows = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futs = [pool.submit(worker, t) for t in tasks]
        for fut in as_completed(futs):
            r = fut.result()
            raw_rows.append(r)
            results[r["tag"]].setdefault(r["instance"], {})[r["seed"]] = r
            if r["status"] == "ok":
                print("[%s] %s seed=%d profit=%.0f runtime=%.1fs"
                      % (r["tag"], r["instance"], r["seed"], r["profit"], r["runtime"]),
                      flush=True)
            else:
                print("[%s] %s seed=%d %s" % (r["tag"], r["instance"], r["seed"], r["status"]),
                      flush=True)

    raw_csv = out_dir / "raw_results.csv"
    with raw_csv.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["instance", "solver", "seed", "profit", "runtime", "status"])
        for r in raw_rows:
            w.writerow([r["instance"], r["tag"], r["seed"],
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
        hw.writerow(["instance", "base_mean", "initant_mean", "mean_diff", "mean_pct",
                     "initant_wins", "base_wins", "ties"])

        print("\n" + "-" * 72)
        print("%-22s %12s %12s %12s %9s" % ("instance", "base", "initant", "diff", "%improve"))
        print("-" * 72)

        for inst in instances:
            stats = {}
            for tag in TAGS:
                rows = [(results[tag][inst][sd]["profit"],
                         results[tag][inst][sd]["runtime"])
                        for sd in results[tag][inst]]
                key = (inst, tag)
                stats[tag] = RX.summarize({key: rows})[key]
                sw.writerow([inst, tag, stats[tag]["n"],
                             "%.6f" % stats[tag]["mean"], "%.6f" % stats[tag]["best"],
                             "%.6f" % stats[tag]["worst"], "%.6f" % stats[tag]["std"],
                             "%.3f" % stats[tag]["runtime_mean"]])

            b, t = stats["base"], stats["initant"]
            diff = t["mean"] - b["mean"]
            pct = 100.0 * diff / b["mean"] if b["mean"] > 0 else 0.0
            hh = RX.head_to_head(
                {sd: results["base"][inst][sd]["profit"] for sd in results["base"][inst]},
                {sd: results["initant"][inst][sd]["profit"] for sd in results["initant"][inst]},
            )
            hw.writerow([inst, "%.6f" % b["mean"], "%.6f" % t["mean"],
                         "%.6f" % diff, "%.4f" % pct,
                         hh["topk_wins"], hh["origin_wins"], hh["ties"]])
            print("%-22s %12.0f %12.0f %12.0f %8.2f%%  (initant %d - %d base, %d tie)"
                  % (inst, b["mean"], t["mean"], diff, pct,
                     hh["topk_wins"], hh["origin_wins"], hh["ties"]))

    print("-" * 72)
    print("Da ghi: %s, %s, %s" % (raw_csv, summary_csv, h2h_csv))


if __name__ == "__main__":
    main()
