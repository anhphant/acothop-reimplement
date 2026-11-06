#!/usr/bin/env python3
"""pla33810_10 ALL: 27 instances x (esaco, origin), tl=900s, NPROC=10.

So sanh esaco vs origin tren TAT CA cac bo test pla33810_10_*.thop.
Same binary, same machine, same seed/params, tasks interleaved (esaco, origin).
KNN init ~5 phut CHAY TRUOC dong ho time-limit -> wall/run ~20 phut.
54 tasks / 10 cores -> est wall ~2h.

Env: NPROC=10 MAX_TIME=900 SEED=10 INSTANCES_DIR OUT_DIR ESACO_BIN
Results -> results/pla33810_10_all_900_ssh/summary.csv
"""
import os, re, csv, subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
INST = Path(os.environ.get("INSTANCES_DIR", ROOT.parent / "instances" / "pla33810-thop"))
OUT = Path(os.environ.get("OUT_DIR", ROOT / "results" / "pla33810_10_all_900_ssh"))
BIN = Path(os.environ.get("ESACO_BIN", ROOT / "build" / "esaco_fix"))

NPROC = int(os.environ.get("NPROC", "10") or "10")
SEED = int(os.environ.get("SEED", "10") or "10")
TL = float(os.environ.get("MAX_TIME", "900") or "900")

def parse_result(text):
    obj = re.search(r"Best objective:\s*(-?(?:\d+(?:\.\d*)?|\.\d+))", text)
    gen = re.search(r"stopping after (\d+) generations", text)
    return obj, gen

def run_one(task):
    instance, variant = task
    code = Path(instance).stem
    OUT.mkdir(parents=True, exist_ok=True)
    out = OUT / f"{code}_{variant}_seed{SEED}.sol"
    log = OUT / f"{code}_{variant}_seed{SEED}.log"
    if log.exists():
        obj, gen = parse_result(log.read_text(errors="replace"))
        if obj is not None:
            return [code, variant, obj.group(1), gen.group(1) if gen else "", 0]
    esaco = 1 if variant == "esaco" else 0
    cmd = [str(BIN), str(instance),
           "--ants", "10", "--time-limit", f"{TL:.6f}",
           "--local-search", "1",
           "--alpha1", "1.0", "--alpha2", "2.0", "--alpha3", "0", "--alpha4", "0",
           "--eps1", "0.1", "--eps2", "0.1", "--rho", "0.5", "--delta", "1",
           "--seed", str(SEED), "--ptries", "1", "--nnants", "20", "--nnls", "20",
           "--esaco", str(esaco), "--esaco2", str(esaco), "--esaco3", str(esaco),
           "--output", str(out)]
    with open(log, "w") as lf:
        rc = subprocess.call(cmd, stdout=lf, stderr=subprocess.STDOUT)
    obj, gen = parse_result(log.read_text(errors="replace"))
    return [code, variant, obj.group(1) if obj else "", gen.group(1) if gen else "", rc]

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    subprocess.run(["g++", "-std=c++17", "-O2",
                    str(ROOT / "src" / "esaco" / "main.cpp"),
                    "-o", str(BIN)], check=True)
    files = sorted(str(p) for p in INST.glob("pla33810_10_*.thop"))
    if not files:
        raise SystemExit(f"no instances in {INST}")
    tasks = [(f, v) for f in files for v in ("esaco", "origin")]
    print(f"instances={len(files)} tasks={len(tasks)} nproc={NPROC} "
          f"tl={TL}s seed={SEED} est wall ~{len(tasks)*20/NPROC/60:.0f} min",
          flush=True)
    csv_path = OUT / "summary.csv"
    with open(csv_path, "w", newline="") as csvf:
        w = csv.writer(csvf)
        w.writerow(["instance", "variant", "best_objective", "generations", "rc"])
        with ThreadPoolExecutor(max_workers=NPROC) as pool:
            for row in pool.map(run_one, tasks):
                w.writerow(row)
                csvf.flush()
                print(f"[{row[0]} {row[1]:6s}] obj={row[2]} gen={row[3]} rc={row[4]}",
                      flush=True)
    print(f"done -> {csv_path}", flush=True)

if __name__ == "__main__":
    main()
