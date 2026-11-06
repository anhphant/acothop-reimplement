#!/usr/bin/env python3
"""Phan tich results/summary.csv (esaco vs origin, 27 config, tl=900s, seed 10).

Ghi results/gap.csv va in thong ke: W/T/L, mean/median gap, theo class/capacity.

Usage: python scripts/analyze.py [SUMMARY_CSV]
"""
import sys
import csv
import statistics
from pathlib import Path
from collections import defaultdict

ROOT = Path(__file__).resolve().parent.parent
SUMMARY = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "results" / "summary.csv"

RECOVERED = {
    "pla33810_10_bsc_01_03": {"origin": 36367106},
    "pla33810_10_unc_01_03": {"origin": 22024948},
    "pla33810_10_bsc_10_02": {"esaco": 209995043},
    "pla33810_10_unc_05_01": {"esaco": 40787176},
    "pla33810_10_usw_01_03": {"esaco": 16413790},
}


def load():
    rows = {}
    with open(SUMMARY, newline="") as f:
        for r in csv.DictReader(f):
            c = rows.setdefault(r["instance"], {})
            c[r["variant"]] = {"obj": float(r["best_objective"]),
                               "gen": int(r["generations"]) if r["generations"] else None}
    for code, fix in RECOVERED.items():
        for variant, obj in fix.items():
            rows.setdefault(code, {}).setdefault(variant, {})["obj"] = float(obj)
    return rows


def pct(a, b):
    return (a - b) / b * 100.0 if b else None


def main():
    rows = load()
    pairs = {c: v for c, v in rows.items() if v.get("esaco") and v.get("origin")}
    print(f"configs={len(pairs)}")

    gaps, w, t, l = [], 0, 0, 0
    with open(ROOT / "results" / "gap.csv", "w", newline="") as f:
        cw = csv.writer(f)
        cw.writerow(["instance", "esaco_obj", "esaco_gen", "origin_obj",
                     "origin_gen", "gap_pct"])
        for c in sorted(pairs):
            e = pairs[c]["esaco"]["obj"]; o = pairs[c]["origin"]["obj"]
            g = pct(e, o)
            gaps.append(g)
            w += e > o; t += e == o; l += e < o
            cw.writerow([c, f"{e:.0f}", pairs[c]["esaco"]["gen"],
                         f"{o:.0f}", pairs[c]["origin"]["gen"], f"{g:+.4f}"])

    print(f"W/T/L = {w}/{t}/{l}")
    print(f"mean gap = {statistics.mean(gaps):+.2f}%  median = {statistics.median(gaps):+.2f}%")
    print(f"best = {max(gaps):+.2f}%  worst = {min(gaps):+.2f}%")

    for label, key in [("class", 2), ("capacity", 3)]:
        by = defaultdict(list)
        for c in sorted(pairs):
            by[c.split("_")[key]].append(pct(pairs[c]["esaco"]["obj"], pairs[c]["origin"]["obj"]))
        print(f"\n== Theo {label} ==")
        for k in sorted(by):
            gs = by[k]
            nw = sum(1 for g in gs if g > 0); nt = sum(1 for g in gs if g == 0)
            print(f"  {k:5s}: n={len(gs):2d}  mean={statistics.mean(gs):+6.2f}%  "
                  f"median={statistics.median(gs):+6.2f}%  W/T/L={nw}/{nt}/{len(gs)-nw-nt}")

    gen_e = [pairs[c]["esaco"]["gen"] for c in pairs if pairs[c]["esaco"]["gen"]]
    gen_o = [pairs[c]["origin"]["gen"] for c in pairs if pairs[c]["origin"]["gen"]]
    print(f"\nGeneration (mean): esaco={statistics.mean(gen_e):.1f}  origin={statistics.mean(gen_o):.1f}")
    print("done -> results/gap.csv")


if __name__ == "__main__":
    main()
