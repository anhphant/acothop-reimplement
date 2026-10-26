#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""
compare_initant_dsj1000.py - So sanh initAnt (gen 1 dung init_ants ant) vs
baseline (gen 1 dung number_of_ants) tren bo dsj1000, dung CUNG 1 binary
original-2phaseconnect-initACO (co flag --init-ants).

    python compare_initant_dsj1000.py --init-ants 1000
    python compare_initant_dsj1000.py --init-ants 1000 --seeds 10 --rebuild

Tat ca tham so ACO nam o block "THAM SO" phia duoi — sua truc tiep o do.
"""

import argparse
import csv
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

from run_experiment import summarize, head_to_head, RE_PROFIT, RE_RUNTIME

BASE_DIR = Path(__file__).resolve().parent

# ============================================================
# THAM SO (SUA O DAY)
# ============================================================
TSP_BASE = "dsj1000"
N_CITIES = 1000

# --- tham so ACO (dsj1000, items=10) ---
ANTS = 500
ALPHA1 = 3
ALPHA2 = 7
ALPHA3 = 0
ALPHA4 = 0
EPS1 = 0.0001
EPS2 = 0.0001
RHO = 0.3
PTRIES = 1
LOCAL_SEARCH = 1
DELTA = 1.0
STEP_ONLINE = 0
DELAYED_ONLINE = 0

# --- item pheromone (2-phase) ---
RHO_ITEM = 0.3
ALPHA_ITEM = 0.0625

# --- initAnt: so ant o generation 1 (None => bat buoc truyen --init-ants) ---
INIT_ANTS = 5000

# --- seeds & thoi gian ---
SEEDS = [269070, 99470, 126489]
TIME_FACTOR = 10.0   # tlimit = N_CITIES * items / TIME_FACTOR

INSTANCES_DIR = BASE_DIR / "instances" / "dsj1000-thop"

# --- binary initAnt ---
INITANT_EXE = (BASE_DIR.parent / "original-2phaseconnect-initACO"
               / "src" / "not-simple-2-tuneopt" / "main.exe")
# ============================================================

TAGS = ["base", "initant"]
CODES = [(10, "bsc", 10, 3), (10, "unc", 10, 3), (10, "usw", 10, 3)]


def instance_name(items, ktype, ksize, ttime):
    return "%s_%02d_%s_%02d_%02d" % (TSP_BASE, items, ktype, ksize, ttime)


def time_limit(items_per_city):
    return N_CITIES * items_per_city / TIME_FACTOR


def resolve_instances_dir(override):
    if override:
        d = Path(override)
        if d.is_dir() and list(d.glob("*.thop")):
            return d
    if INSTANCES_DIR.is_dir() and list(INSTANCES_DIR.glob("*.thop")):
        return INSTANCES_DIR
    print("[ERROR] Khong tim thay thu muc instances %s (chua .thop nao)." % TSP_BASE)
    print("        Da thu: %s" % INSTANCES_DIR)
    sys.exit(1)


def build_cmd(exe, input_path, tlimit, seed, sol_file, log_file,
              init_ants, rho_item, alpha_item):
    cmd = [str(exe), str(input_path),
           "--ants", str(ANTS),
           "--alpha1", str(ALPHA1),
           "--alpha2", str(ALPHA2),
           "--alpha3", str(ALPHA3),
           "--alpha4", str(ALPHA4),
           "--eps1", str(EPS1),
           "--eps2", str(EPS2),
           "--rho", str(RHO),
           "--ptries", str(PTRIES),
           "--local-search", str(LOCAL_SEARCH),
           "--delta", str(DELTA),
           "--step-online", str(STEP_ONLINE),
           "--delayed-online", str(DELAYED_ONLINE),
           "--rho-item", str(rho_item),
           "--alpha-item", str(alpha_item),
           "--init-ants", str(init_ants),
           "--time-limit", "%.1f" % tlimit,
           "--seed", str(seed),
           "--output", str(sol_file),
           "--log", str(log_file)]
    return cmd


def run_one(init_ants, input_path, items, seed, tlimit, out_dir,
            rho_item, alpha_item):
    exe = INITANT_EXE
    name = input_path.stem
    tag = "initant" if init_ants > 0 else "base"
    sol_file = out_dir / "solutions" / ("%s_%s.sol" % (name, tag))
    log_file = out_dir / "logs" / ("%s_%s_seed%d.log" % (name, tag, seed))
    sol_file.parent.mkdir(parents=True, exist_ok=True)
    log_file.parent.mkdir(parents=True, exist_ok=True)

    cmd = build_cmd(exe, input_path, tlimit, seed,
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
        return {"instance": name, "seed": seed, "tag": tag, "profit": profit,
                "runtime": runtime, "status": "error(rc=%d) %s" % (proc.returncode, tail.strip())}

    return {"instance": name, "seed": seed, "tag": tag,
            "profit": profit, "runtime": runtime, "status": status}


def parse_args():
    p = argparse.ArgumentParser(description="initAnt vs baseline tren dsj1000")
    p.add_argument("--init-ants", type=int, default=INIT_ANTS,
                   help="So ant dung o generation 1 (phia initant)")
    p.add_argument("--instances-dir", default=None)
    p.add_argument("--seeds", type=int, default=len(SEEDS))
    p.add_argument("--jobs", type=int, default=8)
    p.add_argument("--outdir", default="results_initant_dsj1000")
    p.add_argument("--rho-item", type=float, default=RHO_ITEM)
    p.add_argument("--alpha-item", type=float, default=ALPHA_ITEM)
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

    if args.init_ants is None or args.init_ants <= 0:
        print("[ERROR] Can --init-ants > 0 (hoac set INIT_ANTS o dau script).")
        sys.exit(1)

    if args.rebuild:
        rebuild()

    if not INITANT_EXE.exists():
        print("[ERROR] Khong tim thay binary: %s (chay --rebuild de compile)" % INITANT_EXE)
        sys.exit(1)

    instances_dir = resolve_instances_dir(args.instances_dir)
    seeds = SEEDS[:args.seeds]

    out_dir = (BASE_DIR / args.outdir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)

    tasks = []
    for (items, ktype, ksize, ttime) in CODES:
        name = instance_name(items, ktype, ksize, ttime)
        input_path = instances_dir / (name + ".thop")
        if not input_path.exists():
            print("[WARN] Bo qua instance thieu file: %s" % input_path)
            continue
        tlimit = time_limit(items)
        # base (init_ants=0) va initant (init_ants=N)
        for init_ants in (0, args.init_ants):
            for seed in seeds:
                tasks.append((init_ants, input_path, items, seed, tlimit, out_dir))

    total = len(tasks)
    print("=" * 72)
    print("  initAnt / baseline  |  %s  |  %d seed(s)  |  init-ants=%d"
          % (TSP_BASE, args.seeds, args.init_ants))
    print("  ants=%d rho=%.4g ptries=%d ls=%d | rho-item=%.4g alpha-item=%.4g"
          % (ANTS, RHO, PTRIES, LOCAL_SEARCH, args.rho_item, args.alpha_item))
    print("  Instances: %d   Tasks: %d  (jobs=%d)" % (len(CODES), total, args.jobs))
    print("  Output   : %s/" % out_dir)
    print("=" * 72)

    results = {tag: {} for tag in TAGS}

    def worker(task):
        init_ants, input_path, items, seed, tlimit, out_dir = task
        r = run_one(init_ants, input_path, items, seed, tlimit, out_dir,
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
                stats[tag] = summarize({key: rows})[key]
                sw.writerow([inst, tag, stats[tag]["n"],
                             "%.6f" % stats[tag]["mean"], "%.6f" % stats[tag]["best"],
                             "%.6f" % stats[tag]["worst"], "%.6f" % stats[tag]["std"],
                             "%.3f" % stats[tag]["runtime_mean"]])

            b, t = stats["base"], stats["initant"]
            diff = t["mean"] - b["mean"]
            pct = 100.0 * diff / b["mean"] if b["mean"] > 0 else 0.0
            hh = head_to_head(
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
