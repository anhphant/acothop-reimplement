#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Ablation --item-heur-interval tren 5 instance dsj1000 ma 2p tung thua dam.

    interval = 1  -> hanh vi cu (compute_heuristic_item moi generation)
    interval = 10/50 -> chi recompute heuristic_item_factor moi N gen

    python ablate_item_heur_interval.py --intervals 1,10,50 --seeds 3 --jobs 8
"""

import argparse
import csv
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from run_experiment import RE_PROFIT, summarize, head_to_head
from compare_2p_origin_dsj1000 import parameter_configurations, random_seeds, BASE_DIR

SRC2P = BASE_DIR / "src-2p" / "not-simple-2-tuneopt"
INSTANCES_DIR = BASE_DIR / "instances" / "dsj1000-thop"

# 5 instance 2p tung thua dam nhat (theo improve.md / results_2p_5lose).
LOSING = [
    "dsj1000_01_unc_10_01",
    "dsj1000_01_usw_05_02",
    "dsj1000_05_bsc_05_01",
    "dsj1000_10_bsc_10_01",
    "dsj1000_10_bsc_10_02",
]


def cfg_key_of(name):
    parts = name.split("_")
    return "dsj1000_%02d_%s" % (int(parts[1]), parts[2])


def time_limit(items):
    return 1000.0 * items / 10.0


def run_one(interval, name, cfg_key, items, seed, out_dir, rho_item, alpha_item):
    exe = SRC2P / "main.exe"
    cmd = [str(exe), str(INSTANCES_DIR / (name + ".thop"))]
    for k, v in parameter_configurations[cfg_key].items():
        cmd += [k, str(v)]
    cmd += ["--rho-item", str(rho_item), "--alpha-item", str(alpha_item),
            "--item-heur-interval", str(interval)]
    cmd += ["--time-limit", "%.1f" % time_limit(items),
            "--seed", str(seed),
            "--output", str(out_dir / "solutions" / ("%s_i%d_%d.sol" % (name, interval, seed))),
            "--log", str(out_dir / "logs" / ("%s_i%d_%d.log" % (name, interval, seed)))]
    proc = subprocess.run(cmd, capture_output=True, text=True,
                          timeout=time_limit(items) + 60, cwd=exe.parent)
    m = RE_PROFIT.search(proc.stdout + "\n" + proc.stderr)
    return float(m.group(1)) if m else None


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--intervals", default="1,10,50")
    p.add_argument("--seeds", type=int, default=3)
    p.add_argument("--jobs", type=int, default=8)
    p.add_argument("--outdir", default="results_ablate_item_heur_interval")
    p.add_argument("--rho-item", type=float, default=0.1)
    p.add_argument("--alpha-item", type=float, default=0.25)
    a = p.parse_args()

    intervals = [int(x) for x in a.intervals.split(",")]
    out_dir = BASE_DIR / a.outdir
    (out_dir / "solutions").mkdir(parents=True, exist_ok=True)
    (out_dir / "logs").mkdir(parents=True, exist_ok=True)

    tasks = []
    for name in LOSING:
        items = int(name.split("_")[1])
        cfg_key = cfg_key_of(name)
        for iv in intervals:
            for seed in random_seeds[:a.seeds]:
                tasks.append((iv, name, cfg_key, items, seed))

    print("item-heur-interval %s  |  %d instance(s)  |  %d seed(s)  |  jobs=%d"
          % (intervals, len(LOSING), a.seeds, a.jobs), flush=True)

    rows = []
    with ThreadPoolExecutor(max_workers=a.jobs) as pool:
        futs = {pool.submit(run_one, *t, out_dir, a.rho_item, a.alpha_item): t
                for t in tasks}
        for fut in as_completed(futs):
            t = futs[fut]
            iv, name, _, items, seed = t
            try:
                profit = fut.result()
            except Exception:
                profit = None
            rows.append((name, iv, seed, profit))
            print("[i%d] %s seed=%d profit=%s"
                  % (iv, name, seed, "" if profit is None else "%.0f" % profit), flush=True)

    csv_path = out_dir / "raw.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["instance", "interval", "seed", "profit"])
        for r in rows:
            w.writerow([r[0], r[1], r[2], "" if r[3] is None else "%.6f" % r[3]])

    base = intervals[0]
    print("\n%-24s %14s %14s %9s %9s"
          % ("instance", "i%d" % base, "i%d" % intervals[-1], "d_last%", "best_i"))
    for name in LOSING:
        by = {}
        for r in rows:
            if r[0] == name and r[3] is not None:
                by.setdefault(r[1], []).append(r[3])
        if len(by) < len(intervals):
            print("%-24s (thieu du lieu)" % name)
            continue
        m = {iv: sum(v) / len(v) for iv, v in by.items()}
        best_iv = max(m, key=m.get)
        print("%-24s %14.0f %14.0f %+8.2f%% %9s"
              % (name, m[base], m[intervals[-1]],
                 100.0 * (m[intervals[-1]] - m[base]) / m[base], "i%d" % best_iv))
    print("\nDa ghi:", csv_path)


if __name__ == "__main__":
    main()
