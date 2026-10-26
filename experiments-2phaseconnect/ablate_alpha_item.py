#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Ablation item pheromone tren src-2p (subset dai dien):
   control  = main.exe      --alpha-item 0      (tat co che - duong tham chieu sach)
   exponent = main_old.exe  --alpha-item 0.25   (co che so mu cu + seed p0 + trail_min cu)
   linear   = main.exe      --alpha-item 0.25   (blend tuyen tinh moi, khong seed, trail_min nong)
   Cung ACO params, seeds, time-limit — chi khac co che item pheromone."""

import argparse
import csv
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from run_experiment import RE_PROFIT
from compare_2p_origin_dsj1000 import parameter_configurations, random_seeds, BASE_DIR

EXE_DIR = BASE_DIR / "src-2p" / "not-simple-2-tuneopt"
INSTANCES_DIR = BASE_DIR / "instances" / "dsj1000-thop"

CONFIGS = {
    "control":  ("main.exe",     ["--rho-item", "0.1", "--alpha-item", "0"]),
    "exponent": ("main_old.exe", ["--rho-item", "0.1", "--alpha-item", "0.25"]),
    "linear":   ("main.exe",     ["--rho-item", "0.1", "--alpha-item", "0.25"]),
}

GROUPS = {
    "01": ["dsj1000_01_bsc_05_02", "dsj1000_01_unc_05_02", "dsj1000_01_usw_05_02"],
    "03": ["dsj1000_03_bsc_05_02", "dsj1000_03_unc_05_02", "dsj1000_03_usw_05_02"],
}


def cfg_key_of(name):
    parts = name.split("_")
    return "dsj1000_%02d_%s" % (int(parts[1]), parts[2])


def run_one(clabel, exe, extra, input_path, cfg_key, tlimit, seed, out_dir):
    cmd = [str(exe), str(input_path)]
    for k, v in parameter_configurations[cfg_key].items():
        cmd += [k, str(v)]
    cmd += extra
    cmd += ["--time-limit", "%.1f" % tlimit,
            "--seed", str(seed),
            "--output", str(out_dir / "solutions" / ("%s_%s_%d.sol" % (input_path.stem, clabel, seed))),
            "--log", str(out_dir / "logs" / ("%s_%s_%d.log" % (input_path.stem, clabel, seed)))]
    proc = subprocess.run(cmd, capture_output=True, text=True,
                          timeout=tlimit + 60, cwd=exe.parent)
    m = RE_PROFIT.search(proc.stdout + "\n" + proc.stderr)
    return float(m.group(1)) if m else None


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--group", default="01", choices=sorted(GROUPS))
    p.add_argument("--seeds", type=int, default=2)
    p.add_argument("--jobs", type=int, default=8)
    p.add_argument("--outdir", default="results_ablate_alpha_item")
    a = p.parse_args()

    out_dir = BASE_DIR / a.outdir / ("group_%s" % a.group)
    (out_dir / "solutions").mkdir(parents=True, exist_ok=True)
    (out_dir / "logs").mkdir(parents=True, exist_ok=True)

    tasks = []
    for name in GROUPS[a.group]:
        items = int(name.split("_")[1])
        for clabel, (exe_name, extra) in CONFIGS.items():
            for seed in random_seeds[:a.seeds]:
                tasks.append((clabel, EXE_DIR / exe_name, extra,
                              INSTANCES_DIR / (name + ".thop"),
                              cfg_key_of(name), 1000 * items / 10.0, seed))

    print("control / exponent / linear  |  %d instance(s)  |  %d seed(s)  |  jobs=%d"
          % (len(GROUPS[a.group]), a.seeds, a.jobs), flush=True)

    rows = []
    with ThreadPoolExecutor(max_workers=a.jobs) as pool:
        futs = {pool.submit(run_one, *t, out_dir): t for t in tasks}
        for fut in as_completed(futs):
            t = futs[fut]
            try:
                profit = fut.result()
            except Exception as e:
                profit = None
            rows.append((t[3].stem, t[0], t[6], profit))
            print("[%s] %s seed=%d profit=%s"
                  % (t[0], t[3].stem, t[6], "" if profit is None else "%.0f" % profit),
                  flush=True)

    csv_path = out_dir / "raw.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["instance", "config", "seed", "profit"])
        for r in rows:
            w.writerow([r[0], r[1], r[2], "" if r[3] is None else "%.6f" % r[3]])

    print("\n%-24s %12s %12s %12s %9s %9s"
          % ("instance", "control", "exponent", "linear", "d_exp%", "d_lin%"))
    for name in GROUPS[a.group]:
        by = {}
        for r in rows:
            if r[0] == name:
                by.setdefault(r[1], []).append(r[3])
        if any(v is None for vv in by.values() for v in vv):
            print("%-24s (thieu du lieu)" % name)
            continue
        m = {c: sum(v) / len(v) for c, v in by.items()}
        print("%-24s %12.0f %12.0f %12.0f %+8.2f%% %+8.2f%%"
              % (name, m["control"], m["exponent"], m["linear"],
                 100.0 * (m["exponent"] - m["control"]) / m["control"],
                 100.0 * (m["linear"] - m["control"]) / m["control"]))
    print("\nDa ghi:", csv_path)


if __name__ == "__main__":
    main()
