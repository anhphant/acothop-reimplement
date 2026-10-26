#!/usr/bin/env python
# -*- coding: utf-8 -*-
"""Ablation quyet dinh (paired, cung seed): origin / control / linear tren dsj1000 items=01.
   Buoc 1 (confound FP):   origin  vs control  - subset 6 instance
   Buoc 2 (paired+sign):   control vs linear   - toan bo 36 instance items=01
   Linear bi GATE (r>0.5, bsc) duoc ghi nhan trong cot gated."""

import argparse
import csv
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from run_experiment import RE_PROFIT
from compare_2p_origin_dsj1000 import parameter_configurations, random_seeds, BASE_DIR

SRC2P = BASE_DIR / "src-2p" / "not-simple-2-tuneopt"
SRCORIGIN = BASE_DIR / "src-origin" / "not-simple-2-tuneopt"
INSTANCES_DIR = BASE_DIR / "instances" / "dsj1000-thop"

SOLVERS = {
    "origin":  (SRCORIGIN / "main.exe", []),
    "control": (SRC2P / "main.exe", ["--rho-item", "0.1", "--alpha-item", "0"]),
    "linear":  (SRC2P / "main.exe", ["--rho-item", "0.1", "--alpha-item", "0.25"]),
}

FP_SUBSET = (["dsj1000_01_bsc_05_%02d" % r for r in (1, 2)] +
             ["dsj1000_01_unc_05_%02d" % r for r in (1, 2)] +
             ["dsj1000_01_usw_05_%02d" % r for r in (1, 2)])


def all01():
    return sorted(p.stem for p in INSTANCES_DIR.glob("dsj1000_01_*.thop"))


def cfg_key_of(name):
    parts = name.split("_")
    return "dsj1000_%02d_%s" % (int(parts[1]), parts[2])


def run_one(solver, exe, extra, input_path, cfg_key, tlimit, seed, out_dir):
    cmd = [str(exe), str(input_path)]
    for k, v in parameter_configurations[cfg_key].items():
        cmd += [k, str(v)]
    cmd += extra
    cmd += ["--time-limit", "%.1f" % tlimit,
            "--seed", str(seed),
            "--output", str(out_dir / "solutions" / ("%s_%s_%d.sol" % (input_path.stem, solver, seed))),
            "--log", str(out_dir / "logs" / ("%s_%s_%d.log" % (input_path.stem, solver, seed)))]
    proc = subprocess.run(cmd, capture_output=True, text=True,
                          timeout=tlimit + 60, cwd=exe.parent)
    out = proc.stdout + "\n" + proc.stderr
    m = RE_PROFIT.search(out)
    return (float(m.group(1)) if m else None, "GATE" in out)


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--solvers", default="origin,control")
    p.add_argument("--group", default="fp_subset", choices=["fp_subset", "all01"])
    p.add_argument("--seeds", type=int, default=2)
    p.add_argument("--jobs", type=int, default=8)
    p.add_argument("--outdir", default="results_ablate_decision")
    a = p.parse_args()

    solvers = [s.strip() for s in a.solvers.split(",")]
    names = FP_SUBSET if a.group == "fp_subset" else all01()

    out_dir = BASE_DIR / a.outdir / a.group
    (out_dir / "solutions").mkdir(parents=True, exist_ok=True)
    (out_dir / "logs").mkdir(parents=True, exist_ok=True)

    tasks = []
    for name in names:
        items = int(name.split("_")[1])
        for s in solvers:
            exe, extra = SOLVERS[s]
            for seed in random_seeds[:a.seeds]:
                tasks.append((s, exe, extra, INSTANCES_DIR / (name + ".thop"),
                              cfg_key_of(name), 1000 * items / 10.0, seed))

    print("%s  |  %s  |  %d instance(s)  |  %d seed(s)  |  jobs=%d"
          % (" vs ".join(solvers), a.group, len(names), a.seeds, a.jobs), flush=True)

    rows = []
    with ThreadPoolExecutor(max_workers=a.jobs) as pool:
        futs = {pool.submit(run_one, *t, out_dir): t for t in tasks}
        for fut in as_completed(futs):
            t = futs[fut]
            try:
                profit, gated = fut.result()
            except Exception:
                profit, gated = None, False
            rows.append((t[3].stem, t[0], t[6], profit, gated))
            print("[%s] %s seed=%d profit=%s%s"
                  % (t[0], t[3].stem, t[6], "" if profit is None else "%.0f" % profit,
                     " GATED" if gated else ""), flush=True)

    csv_path = out_dir / "raw.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["instance", "solver", "seed", "profit", "gated"])
        for r in rows:
            w.writerow([r[0], r[1], r[2], "" if r[3] is None else "%.6f" % r[3],
                        int(r[4])])

    if len(solvers) == 2:
        sa, sb = solvers
        print("\n%-24s %12s %12s %9s  (paired %s w-l)" % ("instance", sa, sb, "diff%", sb))
        wins, losses, ties = 0, 0, 0
        tot_diff = 0.0
        n_ok = 0
        for name in names:
            by = {}
            for r in rows:
                if r[0] == name:
                    by.setdefault(r[1], {})[r[2]] = r[3]
            if (sa not in by or sb not in by or
                    any(v is None for v in by[sa].values()) or
                    any(v is None for v in by[sb].values())):
                print("%-24s (thieu du lieu)" % name)
                continue
            ma = sum(by[sa].values()) / len(by[sa])
            mb = sum(by[sb].values()) / len(by[sb])
            pairs = [(by[sa][sd], by[sb][sd]) for sd in by[sa]]
            w = sum(1 for x, y in pairs if y > x)
            l = sum(1 for x, y in pairs if y < x)
            wins += w
            losses += l
            ties += len(pairs) - w - l
            tot_diff += (mb - ma) / ma
            n_ok += 1
            print("%-24s %12.0f %12.0f %+8.2f%%  (%d - %d)"
                  % (name, ma, mb, 100.0 * (mb - ma) / ma, w, l))
        print("-" * 72)
        print("Tong paired seed-level: %s thang %d, thua %d, hoa %d" % (sb, wins, losses, ties))
        if n_ok:
            print("Trung binh diff%% (theo instance mean): %+.3f%%" % (100.0 * tot_diff / n_ok))
    print("\nDa ghi:", csv_path)


if __name__ == "__main__":
    main()
