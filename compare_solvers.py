#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Chương trình so sánh hiệu năng giữa not-simple-2 và realbench
trên các bộ dữ liệu THOP (Tourist Trip Problem) có hỗ trợ ĐA TIẾN TRÌNH / ĐA LUỒNG (Multithreading/Parallel).

Tác vụ:
- Chạy song song nhiều lượt thử nghiệm trên nhiều lõi CPU (ThreadPoolExecutor)
- Duyệt qua các file instance: dsj1000_10_usw_10_03, dsj1000_10_bsc_10_03, dsj1000_10_unc_10_03
- Thử nghiệm nhiều giá trị Seed khác nhau
- Quét qua dải tham số (alpha: pheromone, beta: heuristic)
- Ghi log thread-safe và xuất kết quả ra file CSV (results_comparison.csv)
- In bảng tổng kết trực quan (Profit cao hơn = Thắng vì bài toán THOP là MAXIMIZE profit).
"""

import os
import re
import csv
import time
import subprocess
import sys
import itertools
import threading
from datetime import datetime
from concurrent.futures import ThreadPoolExecutor, as_completed

sys.stdout.reconfigure(encoding='utf-8')

# ==============================================================================
# 1. CẤU HÌNH ĐƯỜNG DẪN VÀ FILE THỰC THI
# ==============================================================================
BASE_DIR = os.path.dirname(os.path.abspath(__file__))

EXECUTABLES = {
    "not-simple-2": os.path.join(BASE_DIR, "src", "not-simple-2", "main.exe"),
    "realbench":    os.path.join(BASE_DIR, "src", "realbench", "acothop.exe")
}

# 3 bộ dữ liệu yêu cầu
INSTANCES = [
    os.path.join(BASE_DIR, "instances", "dsj1000-thop", "dsj1000_10_usw_10_03.thop"),
    os.path.join(BASE_DIR, "instances", "dsj1000-thop", "dsj1000_01_bsc_01_01.thop"),
    os.path.join(BASE_DIR, "instances", "dsj1000-thop", "dsj1000_01_bsc_01_02.thop"),
]

# Thư mục chứa file solution tạm và file kết quả CSV
OUTPUT_DIR = os.path.join(BASE_DIR, "benchmark_comparison_results")
CSV_RESULT_FILE = os.path.join(BASE_DIR, "results_comparison.csv")
os.makedirs(OUTPUT_DIR, exist_ok=True)

# ==============================================================================
# 2. CẤU HÌNH THỬ NGHIỆM & ĐA LUỒNG
# ==============================================================================

# Số luồng / tiến trình chạy song song (mặc định lấy số core CPU - 2 để tránh đơ máy)
# Bạn có thể tự chỉnh con số này (ví dụ: NUM_WORKERS = 4
NUM_WORKERS = 2
TIME_LIMIT = 900   # Giảm time limit xuống 50s để test nhanh hơn với nhiều cấu hình
ANTS = 500
LOCAL_SEARCH_FLAG = 1

# Danh sách các Seed
SEEDS = [126489, 226489, 326489]

# Các cấu hình Capability (alpha, beta)
ALPHA_CONFIGS = [
    {"alpha": 4.04, "beta": 7.82},
    {"alpha": 1.00, "beta": 2.00},
]

# Các tham số chung khác
# TIME_LIMIT được set ở trên
ANTS = 500                 # Số lượng kiến
LOCAL_SEARCH = 1          # 0: none, 1: 2-opt, 2: 2.5-opt, 3: 3-opt
RHO = 0.39                # Tỷ lệ bay hơi pheromone
PTRIES = 2                # Số lần thử packing mỗi tour

# Tham số bổ sung của not-simple-2
NS_DELTA = 1.0
NS_STEP_ONLINE = 0
NS_DELAYED_ONLINE = 0

# Regex để bóc tách kết quả Objective (Profit) từ Output
REGEX_NOT_SIMPLE_2 = re.compile(r"Best\s+objective:\s*([\d\.\+\-eE]+)", re.IGNORECASE)
REGEX_REALBENCH    = re.compile(r"Best\s+solution:\s*([\d\.\+\-eE]+)", re.IGNORECASE)

# Khóa luồng (Lock) để tránh xung đột khi ghi file CSV và in ra màn hình console
print_lock = threading.Lock()
csv_lock = threading.Lock()

# Biến đếm tiến độ toàn cục
completed_counter = 0
counter_lock = threading.Lock()


# ==============================================================================
# 3. HÀM THỰC THI CHO TỪNG SOLVER
# ==============================================================================

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
                return float(parts[-1])
    except Exception:
        pass
    return None

def run_not_simple_2(exe_path, instance_file, seed, alpha, beta):
    """Chạy not-simple-2/main.exe với đầy đủ tham số dòng lệnh"""
    inst_name = os.path.basename(instance_file)
    sol_file = os.path.join(OUTPUT_DIR, f"ns2_{inst_name}_s{seed}_a_{alpha}_b_{beta}.sol")

    cmd = [
        exe_path,
        instance_file,
        "--ants",           str(ANTS),
        "--time-limit",     str(TIME_LIMIT),
        "--local-search",   str(LOCAL_SEARCH),
        "--alpha1",         str(alpha),
        "--alpha2",         str(beta),
        "--alpha3",         "0.001",
        "--alpha4",         "0.001",
        "--eps1",           "0.1",
        "--eps2",           "0.1",
        "--rho",            str(RHO),
        "--delta",          str(NS_DELTA),
        "--seed",           str(seed),
        "--output",         sol_file,
        "--ptries",         str(PTRIES),
        "--step-online",    str(NS_STEP_ONLINE),
        "--delayed-online", str(NS_DELAYED_ONLINE)
    ]

    start_t = time.time()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=TIME_LIMIT + 45)
        elapsed = time.time() - start_t
        out = proc.stdout + "\n" + proc.stderr

        match = REGEX_NOT_SIMPLE_2.search(out)
        profit = check_solution_profit(instance_file, sol_file)
        if profit is None and match:
            profit = float(match.group(1))

        return {
            "solver": "not-simple-2",
            "profit": profit,
            "time": elapsed,
            "status": "OK" if profit is not None else "PARSE_FAIL",
            "cmd": " ".join(cmd)
        }
    except subprocess.TimeoutExpired:
        return {"solver": "not-simple-2", "profit": None, "time": TIME_LIMIT + 15, "status": "TIMEOUT", "cmd": " ".join(cmd)}
    except Exception as e:
        return {"solver": "not-simple-2", "profit": None, "time": time.time() - start_t, "status": f"ERROR: {str(e)[:30]}", "cmd": " ".join(cmd)}


def run_realbench(exe_path, instance_file, seed, alpha, beta):
    """Chạy realbench/acothop.exe"""
    inst_name = os.path.basename(instance_file)
    sol_file = os.path.join(OUTPUT_DIR, f"rb_{inst_name}_s{seed}_a_{alpha}_b_{beta}.sol")

    cmd = [
        exe_path,
        "--mmas",
        "--tries",        "1",
        "--ants",         str(ANTS),
        "--time",         str(TIME_LIMIT),
        "--localsearch",  str(LOCAL_SEARCH),
        "--alpha",        str(alpha),
        "--beta",         str(beta),
        "--rho",          str(RHO),
        "--ptries",       str(PTRIES),
        "--seed",         str(seed),
        "--inputfile",    instance_file,
        "--outputfile",   sol_file
    ]

    start_t = time.time()
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=TIME_LIMIT + 45)
        elapsed = time.time() - start_t
        out = proc.stdout + "\n" + proc.stderr

        match = REGEX_REALBENCH.search(out)
        profit = check_solution_profit(instance_file, sol_file)
        if profit is None and match:
            profit = float(match.group(1))

        return {
            "solver": "realbench",
            "profit": profit,
            "time": elapsed,
            "status": "OK" if profit is not None else "PARSE_FAIL",
            "cmd": " ".join(cmd)
        }
    except subprocess.TimeoutExpired:
        return {"solver": "realbench", "profit": None, "time": TIME_LIMIT + 15, "status": "TIMEOUT", "cmd": " ".join(cmd)}
    except Exception as e:
        return {"solver": "realbench", "profit": None, "time": time.time() - start_t, "status": f"ERROR: {str(e)[:30]}", "cmd": " ".join(cmd)}


# ==============================================================================
# 4. HÀM XỬ LÝ CHO 1 TÁC VỤ (WORKER TASK)
# ==============================================================================

def execute_single_comparison(task_info):
    """
    Một đơn vị công việc (Task): Chạy cả 2 solver cho cùng một (instance, seed, alpha, beta)
    và so sánh kết quả.
    """
    global completed_counter
    inst, seed, alpha, beta, total_tasks = task_info
    inst_name = os.path.basename(inst)

    # 1. Chạy not-simple-2
    res_ns = run_not_simple_2(EXECUTABLES["not-simple-2"], inst, seed, alpha, beta)

    # 2. Chạy realbench
    res_rb = run_realbench(EXECUTABLES["realbench"], inst, seed, alpha, beta)

    # 3. So sánh (THOP là Maximize Profit: profit cao hơn là Thắng)
    p_ns = res_ns["profit"]
    p_rb = res_rb["profit"]

    if p_ns is not None and p_rb is not None:
        diff = p_ns - p_rb
        if p_ns > p_rb:
            winner = "not-simple-2 (+)"
        elif p_rb > p_ns:
            winner = "realbench (+)"
        else:
            winner = "TIE (Hòa)"
    elif p_ns is not None:
        winner = "not-simple-2"
        diff = 0
    elif p_rb is not None:
        winner = "realbench"
        diff = 0
    else:
        winner = "BOTH_FAILED"
        diff = 0

    # Cập nhật số thứ tự hoàn thành
    with counter_lock:
        completed_counter += 1
        idx = completed_counter

    # Chuỗi hiển thị kết quả
    val_ns_str = f"{p_ns:.2f} ({res_ns['time']:.1f}s)" if p_ns is not None else f"FAIL ({res_ns['status']})"
    val_rb_str = f"{p_rb:.2f} ({res_rb['time']:.1f}s)" if p_rb is not None else f"FAIL ({res_rb['status']})"

    # In ra terminal an toàn với Lock
    with print_lock:
        progress_str = f"[{idx}/{total_tasks}]"
        print(f"{progress_str:<8} | {inst_name:<24} | {seed:<8} | ({alpha},{beta}){'':<4} | {val_ns_str:<16} | {val_rb_str:<16} | {winner:<16}", flush=True)

    # Ghi vào file CSV an toàn với Lock
    with csv_lock:
        with open(CSV_RESULT_FILE, mode="a", newline="", encoding="utf-8") as f:
            writer = csv.writer(f)
            writer.writerow([
                datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                inst_name, seed, alpha, beta,
                p_ns, f"{res_ns['time']:.2f}", res_ns['status'],
                p_rb, f"{res_rb['time']:.2f}", res_rb['status'],
                winner, f"{diff:.2f}"
            ])

    return {
        "instance": inst_name,
        "seed": seed,
        "alpha": alpha, "beta": beta,
        "ns_profit": p_ns, "ns_time": res_ns["time"],
        "rb_profit": p_rb, "rb_time": res_rb["time"],
        "winner": winner
    }


# ==============================================================================
# 5. CHƯƠNG TRÌNH CHÍNH (MAIN BENCHMARK RUNNER ĐA LUỒNG)
# ==============================================================================

def main():
    global completed_counter
    completed_counter = 0

    print("=" * 110)
    print(" " * 24 + "THOP SOLVERS BENCHMARK: not-simple-2 vs realbench (PARALLEL)")
    print("=" * 110)
    print(f"Time Limit: {TIME_LIMIT}s | Ants: {ANTS} | LocalSearch: {LOCAL_SEARCH} | Ptries: {PTRIES}")
    print(f"Chế độ chạy: ĐA LUỒNG (Multithreading) với NUM_WORKERS = {NUM_WORKERS} workers song song")
    print(f"Executables:")
    print(f"  - not-simple-2: {EXECUTABLES['not-simple-2']}")
    print(f"  - realbench   : {EXECUTABLES['realbench']}")
    print("=" * 110)

    # Kiểm tra sự tồn tại của file exe
    for s_name, path in EXECUTABLES.items():
        if not os.path.exists(path):
            print(f"[CẢNH BÁO] Không tìm thấy file thực thi cho {s_name}: {path}")
            print("Vui lòng kiểm tra lại đường dẫn hoặc biên dịch lại trước khi chạy.")
            return

    # Khởi tạo file CSV
    with open(CSV_RESULT_FILE, mode="w", newline="", encoding="utf-8") as f:
        writer = csv.writer(f)
        writer.writerow([
            "timestamp", "instance", "seed", "alpha", "beta",
            "ns2_profit", "ns2_time", "ns2_status",
            "rb_profit", "rb_time", "rb_status",
            "winner", "diff_profit"
        ])

    # Tạo danh sách tất cả các tác vụ cần chạy
    task_list = []
    for inst in INSTANCES:
        if not os.path.exists(inst):
            print(f"[BỎ QUA] Không tìm thấy instance: {inst}")
            continue
        for alpha_cfg in ALPHA_CONFIGS:
            a, b = alpha_cfg["alpha"], alpha_cfg["beta"]
            for seed in SEEDS:
                task_list.append((inst, seed, a, b))

    total_tasks = len(task_list)
    # Gắn thêm total_tasks vào từng task để in progress
    tasks_with_meta = [(inst, seed, a, b, total_tasks) for (inst, seed, a, b) in task_list]

    print(f"\nBắt đầu chạy {total_tasks} lượt thử nghiệm trên {NUM_WORKERS} workers...\n")
    header_fmt = f"{'Tiến độ':<8} | {'Instance':<24} | {'Seed':<8} | {'(a, b)':<10} | {'not-simple-2':<16} | {'realbench':<16} | {'Winner':<16}"
    print(header_fmt)
    print("-" * len(header_fmt))

    all_comparisons = []
    start_total_time = time.time()

    # Thực thi đa luồng
    with ThreadPoolExecutor(max_workers=NUM_WORKERS) as executor:
        futures = [executor.submit(execute_single_comparison, t) for t in tasks_with_meta]
        for fut in as_completed(futures):
            try:
                res = fut.result()
                all_comparisons.append(res)
            except Exception as e:
                print(f"[LỖI THREAD]: {e}")

    total_wall_time = time.time() - start_total_time

    # ==============================================================================
    # 6. TỔNG KẾT VÀ THỐNG KÊ (SUMMARY)
    # ==============================================================================
    print("\n" + "=" * 90)
    print(" " * 30 + "BẢNG TỔNG KẾT HIỆU NĂNG")
    print("=" * 90)

    summary = {
        "not-simple-2": {"wins": 0, "total_profit": 0.0, "total_time": 0.0, "valid_runs": 0},
        "realbench":     {"wins": 0, "total_profit": 0.0, "total_time": 0.0, "valid_runs": 0},
        "ties": 0
    }

    for c in all_comparisons:
        if "not-simple-2 (+)" in c["winner"]:
            summary["not-simple-2"]["wins"] += 1
        elif "realbench (+)" in c["winner"]:
            summary["realbench"]["wins"] += 1
        elif "TIE" in c["winner"]:
            summary["ties"] += 1

        if c["ns_profit"] is not None:
            summary["not-simple-2"]["total_profit"] += c["ns_profit"]
            summary["not-simple-2"]["total_time"] += c["ns_time"]
            summary["not-simple-2"]["valid_runs"] += 1

        if c["rb_profit"] is not None:
            summary["realbench"]["total_profit"] += c["rb_profit"]
            summary["realbench"]["total_time"] += c["rb_time"]
            summary["realbench"]["valid_runs"] += 1

    sum_fmt = f"{'Solver':<16} | {'Số lần thắng (Max Profit)':<28} | {'Avg Profit':<15} | {'Avg Time (s)':<12}"
    print(sum_fmt)
    print("-" * len(sum_fmt))

    for s_name in ["not-simple-2", "realbench"]:
        v_runs = summary[s_name]["valid_runs"]
        avg_p = (summary[s_name]["total_profit"] / v_runs) if v_runs > 0 else 0
        avg_t = (summary[s_name]["total_time"] / v_runs) if v_runs > 0 else 0
        print(f"{s_name:<16} | {summary[s_name]['wins']:<28} | {avg_p:<15.2f} | {avg_t:<12.2f}")

    print(f"\n- Số lần hòa (Ties): {summary['ties']}")
    print(f"- Tổng thời gian chạy thực tế (Wall-clock time): {total_wall_time:.2f} giây")
    print(f"- Chi tiết từng lượt chạy đã được lưu tại: {CSV_RESULT_FILE}")
    print("=" * 90)


if __name__ == "__main__":
    main()
