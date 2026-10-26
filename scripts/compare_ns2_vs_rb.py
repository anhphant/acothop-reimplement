#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Chương trình so sánh hiệu năng giữa not-simple-2-tuneopt và realbench
trên cùng 1 bộ tham số, nhưng với nhiều seed khác nhau.
- Multithreading 4 workers.
"""

import os
import re
import csv
import time
import subprocess
import sys
import threading
from datetime import datetime
from concurrent.futures import ThreadPoolExecutor, as_completed

sys.stdout.reconfigure(encoding='utf-8')

BASE_DIR = os.path.dirname(os.path.abspath(__file__))

EXECUTABLES = {
    "not-simple-2-tuneopt": os.path.join(BASE_DIR, "src", "not-simple-2-tuneopt", "main.exe"),
    "realbench":            os.path.join(BASE_DIR, "src", "realbench", "acothop.exe")
}

INSTANCES = [
    os.path.join(BASE_DIR, "instances", "dsj1000-thop", "dsj1000_10_unc_10_03.thop"),
    os.path.join(BASE_DIR, "instances", "dsj1000-thop", "dsj1000_10_usw_10_03.thop"),
    os.path.join(BASE_DIR, "instances", "dsj1000-thop", "dsj1000_10_bsc_10_03.thop")
]

OUTPUT_DIR = os.path.join(BASE_DIR, "benchmark_ns2_vs_rb_multi")
CSV_RESULT_FILE = os.path.join(BASE_DIR, "results_ns2_vs_rb_multi.csv")
os.makedirs(OUTPUT_DIR, exist_ok=True)

NUM_WORKERS = 4
TIME_LIMIT = 998  # Chạy 30s để tiết kiệm thời gian do có nhiều instance
SEEDS = [1990, 326489, 526489, 726489]

FIXED_PARAMS = {
    "ants": 500,
    "ptries": 2,
    "localsearch": 1,
    "alpha1": 4.04,
    "alpha2": 7.82,
    "rho": 0.39,
    "alpha3": 0.0,
    "alpha4": 0.795394,
    "eps1": 0.0001,
    "eps2": 0.0001,
    "delta": 1.0,
    "mmas": 1
}

print_lock = threading.Lock()
csv_lock = threading.Lock()
completed_counter = 0
counter_lock = threading.Lock()

def check_solution_profit(instance_file, sol_file):
    checker_script = os.path.join(BASE_DIR, "solutions", "thop_solution_checker.py")
    if not os.path.exists(checker_script) or not os.path.exists(sol_file):
        return None
    try:
        cmd = [sys.executable, checker_script, instance_file, sol_file]
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=15)
        out = proc.stdout.strip()
        if out:
            parts = out.split()
            if len(parts) >= 3:
                try:
                    return float(parts[-1])
                except:
                    pass
    except Exception:
        pass
    return None

def run_ns2_tuneopt(exe_path, instance_file, seed):
    inst_name = os.path.basename(instance_file)
    sol_file = os.path.join(OUTPUT_DIR, f"ns2_{inst_name}_s{seed}.sol")

    cmd = [
        exe_path, instance_file,
        "--ants", str(FIXED_PARAMS["ants"]),
        "--time-limit", str(TIME_LIMIT),
        "--local-search", str(FIXED_PARAMS["localsearch"]),
        "--alpha1", str(FIXED_PARAMS["alpha1"]),
        "--alpha2", str(FIXED_PARAMS["alpha2"]),
        "--alpha3", str(FIXED_PARAMS["alpha3"]),
        "--alpha4", str(FIXED_PARAMS["alpha4"]),
        "--eps1", str(FIXED_PARAMS["eps1"]),
        "--eps2", str(FIXED_PARAMS["eps2"]),
        "--rho", str(FIXED_PARAMS["rho"]),
        "--delta", str(FIXED_PARAMS["delta"]),
        "--seed", str(seed),
        "--output", sol_file,
        "--ptries", str(FIXED_PARAMS["ptries"]),
        "--step-online", "0",
        "--delayed-online", "0"
    ]

    start_t = time.time()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=TIME_LIMIT + 600)
        elapsed = time.time() - start_t
        profit = check_solution_profit(instance_file, sol_file)
        if profit is None:
            m = re.search(r"Best\s+objective:\s*([\d\.\+\-eE]+)", proc.stdout)
            if m: profit = float(m.group(1))

        return {"profit": profit, "time": elapsed, "status": "OK" if profit else "FAIL"}
    except subprocess.TimeoutExpired:
        return {"profit": None, "time": TIME_LIMIT + 600, "status": "TIMEOUT"}
    except Exception as e:
        return {"profit": None, "time": time.time() - start_t, "status": f"ERROR"}


def run_realbench(exe_path, instance_file, seed):
    inst_name = os.path.basename(instance_file)
    sol_file = os.path.join(OUTPUT_DIR, f"rb_{inst_name}_s{seed}.sol")

    cmd = [
        exe_path,
        "--mmas",
        "--tries", "1",
        "--ants", str(FIXED_PARAMS["ants"]),
        "--time", str(TIME_LIMIT),
        "--localsearch", str(FIXED_PARAMS["localsearch"]),
        "--alpha", str(FIXED_PARAMS["alpha1"]),
        "--beta", str(FIXED_PARAMS["alpha2"]),
        "--rho", str(FIXED_PARAMS["rho"]),
        "--ptries", str(FIXED_PARAMS["ptries"]),
        "--seed", str(seed),
        "--inputfile", instance_file,
        "--outputfile", sol_file
    ]

    start_t = time.time()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=TIME_LIMIT + 600)
        elapsed = time.time() - start_t
        profit = check_solution_profit(instance_file, sol_file)
        
        return {"profit": profit, "time": elapsed, "status": "OK" if profit else "FAIL"}
    except subprocess.TimeoutExpired:
        return {"profit": None, "time": TIME_LIMIT + 600, "status": "TIMEOUT"}
    except Exception as e:
        return {"profit": None, "time": time.time() - start_t, "status": f"ERROR"}


def execute_single_comparison(task_info):
    global completed_counter
    inst, seed, total_tasks = task_info
    inst_name = os.path.basename(inst)

    res_ns = run_ns2_tuneopt(EXECUTABLES["not-simple-2-tuneopt"], inst, seed)
    res_rb = run_realbench(EXECUTABLES["realbench"], inst, seed)

    p_ns = res_ns["profit"]
    p_rb = res_rb["profit"]

    if p_ns is not None and p_rb is not None:
        diff = p_ns - p_rb
        if diff > 0.001: winner = "not-simple-2 (+)"
        elif diff < -0.001: winner = "realbench (+)"
        else: winner = "TIE (Hoa)"
    elif p_ns: winner = "not-simple-2"
    elif p_rb: winner = "realbench"
    else: winner = "BOTH_FAIL"

    with counter_lock:
        completed_counter += 1
        idx = completed_counter

    val_ns_str = f"{p_ns:.0f} ({res_ns['time']:.1f}s)" if p_ns else f"FAIL ({res_ns['status']})"
    val_rb_str = f"{p_rb:.0f} ({res_rb['time']:.1f}s)" if p_rb else f"FAIL ({res_rb['status']})"

    with print_lock:
        print(f"[{idx}/{total_tasks}] | {inst_name[:12]:<12} | {seed:<6} | {val_ns_str:<18} | {val_rb_str:<18} | {winner:<16}", flush=True)

    with csv_lock:
        with open(CSV_RESULT_FILE, mode="a", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow([
                datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                inst_name, seed, p_ns, res_ns['time'], res_ns['status'],
                p_rb, res_rb['time'], res_rb['status'], winner
            ])

def main():
    print("=" * 90)
    print(" " * 15 + "BENCHMARK: not-simple-2-tuneopt vs realbench")
    print("=" * 90)
    print(f"Time Limit : {TIME_LIMIT}s")
    print(f"Workers    : {NUM_WORKERS} threads")
    print(f"Fixed      : Ants={FIXED_PARAMS['ants']}, PTries={FIXED_PARAMS['ptries']}, LS={FIXED_PARAMS['localsearch']}, alpha1={FIXED_PARAMS['alpha1']}, alpha2={FIXED_PARAMS['alpha2']}")
    
    for name, exe in EXECUTABLES.items():
        if not os.path.exists(exe):
            print(f"[Loi] Khong tim thay {name} tai: {exe}")
            return
            
    if not os.path.exists(CSV_RESULT_FILE):
        with open(CSV_RESULT_FILE, mode="w", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow(["Timestamp", "Instance", "Seed", 
                             "NS2_Profit", "NS2_Time", "NS2_Status",
                             "RB_Profit", "RB_Time", "RB_Status", "Winner"])

    tasks = []
    for inst in INSTANCES:
        for seed in SEEDS:
            tasks.append((inst, seed))
            
    total = len(tasks)
    print(f"Total Runs : {total} comparisons\n")
    print("-" * 90)
    print(f"{'Progress':<8} | {'Instance':<12} | {'Seed':<6} | {'ns2-tuneopt':<18} | {'realbench':<18} | {'Winner':<16}")
    print("-" * 90)
    
    full_tasks = [(t[0], t[1], total) for t in tasks]

    with ThreadPoolExecutor(max_workers=NUM_WORKERS) as executor:
        futures = [executor.submit(execute_single_comparison, t) for t in full_tasks]
        for f in as_completed(futures):
            f.result()

    print("-" * 90)
    print("HOAN THANH. Xem chi tiet trong:", CSV_RESULT_FILE)

if __name__ == "__main__":
    main()
