# Hướng dẫn chạy toàn bộ dsj1000 trên máy trường (Ubuntu)

Script `compare_2p_origin_dsj1000.py` chạy **so sánh 2 solver**:

| solver   | binary                                | ghi chú                     |
|----------|---------------------------------------|------------------------------|
| `2p`     | `src-2p/not-simple-2-tuneopt/main.exe`     | có item pheromone (2-phase)  |
| `origin` | `src-origin/not-simple-2-tuneopt/main.exe` | baseline (1-phase)           |

- **117 instance** dsj1000 (36 x items=1, 27 x items=3, 27 x items=5, 27 x items=10)
- **3 seeds**: `269070, 99470, 126489`
- **8 cores** chạy song song
- **Tham số cố định** (lấy từ `note.txt`, tuned trên `dsj1000_10_usw_10_03`), áp dụng chung cho mọi instance.

---

## 1. Chuẩn bị trên máy Ubuntu (chạy 1 lần)

```bash
sudo apt update
sudo apt install -y build-essential   # cài g++ (python3 có sẵn trên Ubuntu)
```

---

## 2. Copy code lên máy trường

Từ máy Windows, mở **Git Bash** tại thư mục project, gói gọn những thứ cần thiết rồi `scp`:

```bash
cd /d/PhD/THOP-TTP/acothop/exp-2phaseconnect-ssh

# Gói (bỏ file .exe/.log/.sol/.db nặng)
tar czf dsj1000_run.tar.gz \
  --exclude='*.exe' --exclude='*.log' --exclude='*.sol' --exclude='*.db' \
  --exclude='*.txt' --exclude='__pycache__' --exclude='outputs' \
  src-2p src-origin instances/dsj1000-thop \
  compare_2p_origin_dsj1000.py run_experiment.py guide.md

# Copy lên máy trường (thay user/maytruong bằng tài khoản thật)
scp dsj1000_run.tar.gz user@maytruong:/home/user/
```

Trên máy trường, giải nén:

```bash
cd ~
tar xzf dsj1000_run.tar.gz        # tạo ra src-2p/, src-origin/, instances/, *.py
```

> Cấu trúc sau giải nén phải là: `compare_2p_origin_dsj1000.py`, `run_experiment.py`,
> `instances/dsj1000-thop/*.thop`, `src-2p/not-simple-2-tuneopt/main.cpp`, `src-origin/...`.

---

## 3. Smoke test nhanh (khuyên dùng — kiểm tra build + chạy trước khi chạy cả đêm)

Compile và chạy thử 1 instance nhỏ (items=1) trong 10 giây:

```bash
cd ~/src-2p/not-simple-2-tuneopt
g++ -std=c++17 -O2 main.cpp -o main.exe

./main.exe ../../instances/dsj1000-thop/dsj1000_01_bsc_01_01.thop \
  --ants 500 --alpha1 1.7568 --alpha2 5.1299 --alpha3 1.3617 --alpha4 0.2531 \
  --eps1 125.8501 --eps2 24948.57 --rho 0.1468 --ptries 3 --local-search 1 \
  --delta 1.0 --step-online 0 --delayed-online 0 \
  --rho-item 0.3 --alpha-item 0.0625 \
  --time-limit 10 --seed 269070 --output /tmp/smoke.sol --log /tmp/smoke.log
```

Nếu in ra `Best objective: ...` và `Runtime: ...` là OK.

---

## 4. Chạy chính thức (toàn bộ instance, 3 seeds, 8 cores)

Chạy trong `tmux` (hoặc `nohup`) để không bị ngắt khi mất kết nối SSH:

```bash
cd ~                      # thư mục chứa compare_2p_origin_dsj1000.py

tmux new -s dsj          # mở phiên tmux (detach bằng Ctrl+B rồi D)

# Build cả 2 solver rồi chạy toàn bộ (3 seeds, 8 core song song)
python3 compare_2p_origin_dsj1000.py --rebuild --jobs 8 2>&1 | tee run_all.log

# Khi cần quay lại xem:
#   tmux attach -t dsj
```

Không muốn dùng tmux thì thay bằng `nohup`:

```bash
nohup python3 compare_2p_origin_dsj1000.py --rebuild --jobs 8 > run_all.log 2>&1 &
tail -f run_all.log
```

---

## 5. Kết quả nằm ở đâu

Tất cả ghi vào thư mục `results_2p_origin_dsj1000/`:

| file                          | nội dung                                          |
|-------------------------------|---------------------------------------------------|
| `raw_results.csv`             | từng run: instance, solver, seed, profit, runtime |
| `summary.csv`                 | mean/best/worst/std cho mỗi (instance, solver)    |
| `head_to_head.csv`            | so sánh 2p vs origin theo instance                |
| `solutions/*.sol`             | lời giải từng run                                 |
| `logs/*.log`                  | log chi tiết từng run                             |

Sau khi chạy xong, copy kết quả về máy Windows:

```bash
# từ Git Bash trên Windows
scp -r user@maytruong:/home/user/results_2p_origin_dsj1000 .
```

---

## 6. Thời gian ước tính

- Tổng CPU: **~87 giờ** (117 instance × 3 seeds × 2 solvers).
- Với **8 core** chạy song song: **~11 giờ thực tế** (có thể 12–14h tuỳ máy).

Time limit mỗi run = `1000 × items / 10` giây → items=1: 100s, items=3: 300s, items=5: 500s, items=10: 1000s.

---

## 7. Tuỳ chỉnh (nếu cần)

| nhu cầu                        | cách làm                                                          |
|--------------------------------|-------------------------------------------------------------------|
| đổi số core                    | sửa `--jobs 8` (vd `--jobs 16`)                                   |
| đổi số seed                    | sửa `--seeds 10`                                                  |
| đổi tham số ACO                | sửa block `PARAMS` / `FIXED` đầu file `compare_2p_origin_dsj1000.py` |
| đổi thư mục kết quả            | thêm `--outdir ten_thu_muc`                                       |
| chỉ chạy một nhóm instance nhỏ | lọc trong `discover_instances()` hoặc dùng `--instances-dir` trỏ tới thư mục con |

---

## Tham số cố định đang dùng

```
--ants 500 --alpha1 1.7568 --alpha2 5.1299 --alpha3 1.3617 --alpha4 0.2531
--eps1 125.8501 --eps2 24948.57 --rho 0.1468 --ptries 3 --local-search 1
--delta 1.0 --step-online 0 --delayed-online 0
--rho-item 0.3 --alpha-item 0.0625   (chỉ solver 2p)
```
