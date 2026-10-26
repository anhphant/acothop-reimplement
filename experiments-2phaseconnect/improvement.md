# src-2p vs src-origin — những gì đã cải thiện (cập nhật theo code hiện tại)

`src-origin`: chỉ có pheromone **tour** (1-phase), mảng tĩnh `MAX_CITIES = 4000` → không tải nổi instance > 4000 thành phố.

`src-2p` làm 2 việc:
1. **Refactor** để chạy instance lớn (d15112, pla33810…) — **cải thiện thật, đã kiểm chứng.**
2. **Thêm item pheromone 2-phase** — nhưng sau thực nghiệm, item pheromone **không cải thiện chất lượng**
   (trơ trên unc/usw, bị gate tắt trên bsc). → về chất lượng lời giải, src-2p hiện **hòa origin**.

Kết luận gọn: src-2p **chỉ hơn origin ở khả năng chạy instance lớn**, không hơn về objective.

---

## 1. Refactor — cải thiện THẬT (đã kiểm chứng)

### 1.1 Ma trận thưa (sparse candidate-list)

Pheromone/heuristic từ bảng `n×n` → chỉ lưu `n·K` láng giềng gần (`K≈20`), index theo slot `i*K + k` trỏ tới `knn[i][k]`:

```cpp
vector<double> phe_;        // n*K
vector<double> total_;      // n*K
vector<double> heuristic_;  // n*K
inline double& PHE(int i, int k) { return phe_[i * K + k]; }
```

### 1.2 Mảng runtime + khoảng cách on-the-fly

Bỏ cap tĩnh `MAX_CITIES/MAX_ITEMS/MAX_ANTS/MAX_KN` (origin là 4000), cấp phát theo đúng kích thước instance;
bỏ bảng khoảng cách `n×n`, tính `sqrt`+`ceil` khi cần (`thopproblem.cpp`).

### 1.3 Tái dùng `knn` cho Local Search

LS không sort lại khoảng cách (`O(n² log n)` → `O(n·k)`), chỉ ánh xạ city-id → vị trí trong tour và lọc thành phố ngoài tour.

### 1.4 Micro-opt

Tính trước `pow`/`sqrt` tránh lặp trong vòng nóng: bảng tra `dist_pow_[d]`, khóa sort log thay `pow(profit,θ)` (`packing.cpp`).

### 1.5 Sửa OOB (đọc/ghi vượt mảng khi instance lớn)

`colony.cpp` cấp slot dummy, `ant.cpp` pre-size `sequence`, `thopproblem.cpp` topk có slot dummy.

**Bằng chứng trung thành:** `--alpha-item 0` (và instance `cap=inf`) → src-2p khớp origin **từng bit**
(`results_ablate_decision/fp_subset`: origin ≈ control). Refactor không làm sai lệch thuật toán gốc.

---

## 2. Item pheromone 2-phase — thêm nhưng KHÔNG thắng

### 2.1 Cơ chế hiện tại (khác bản exponent đầu)

**a) Chuẩn hóa linear blend (I1)** — thay `pow(sum_rw, phe_norm)` bằng dạng scale-invariant, bị chặn chặt
(`pheromone.cpp:110-118`):

```
heuristic_item_factor[i] = 1 + α_item · phe_norm(i) · 10 · (sum_rw(i) / max_rw)
   phe_norm(i) = Σ phe_item / (|I(i)| · item_trail_max)   ∈ [0,1]
   → factor ∈ [1, 1 + 10·α_item]
```

**b) Gate c3** (`main.cpp:160-178`): tương quan profit–weight `r > 0.5` → `α_item = 0`
(tắt item pheromone; bsc có r ≈ 0.996 nên bị tắt).

**c) Warm-up** (`main.cpp:208-229`): 10% thời gian đầu `α_item = 0` (tour ổn định trước);
hết warm-up chạy `packIterative` 1 lần (đã port từ worktree, **không seed p0** — giữ item pheromone uniform).

**d) Packing bias (I4)** (`packing.cpp:174-176`): item pheromone cũng đẩy sort-key packing, không chỉ bẻ tour:

```cpp
double key = delta·log_weight[j] + gamma·log_d_end[node] - theta·log_profit[j] - α_item·bias;
// bias[j] = phe_item[j]/item_trail_max ∈ [0,1], xuất mỗi generation
```

**e) Trail-min "nóng"**: `item_trail_min = item_trail_max / √(2·nItems)` (thay `max/2n`).

### 2.2 Vì sao không thắng (bằng chứng)

| Thí nghiệm | Kết quả | Kết luận |
|---|---|---|
| `results_2p_origin_dsj1000` (117 inst, bản exponent cũ) | 2p thua nhẹ −0.10% | item pheromone vô hại → có hại khi n lớn |
| `results_ablate_alpha_item` (control/exponent/linear) | linear ≈ control | linear blend trơ |
| `results_ablate_decision` (36 inst items=01) | origin ≈ control ≈ linear | refactor trung thành + item trơ |
| `tune_rhoitem_alphaitem` (30 điểm) | α_item>0 chỉ ±0.7% | tuning = nhiễu |
| `results_ablate_item_heur_interval` (i1/i5/i10/i50) | bsc bị gate → giống nhau; unc/usw ±0.4% = nhiễu | interval không tác dụng |

Gốc rễ: trên bsc p/w ≈ hằng → **không có tín hiệu** (gate đúng); trên unc/usw tín hiệu item
**đã có sẵn** trong heuristic thành phố (`alpha3/alpha4`) + packing greedy → item pheromone dư thừa.

---

## 3. Tham số / flag mới

| Flag | Mặc định | Ý nghĩa |
|---|---|---|
| `--rho-item` | 0.05 | bay hơi item pheromone |
| `--alpha-item` | 1.0 | độ mạnh item pheromone |
| `--item-heur-interval` | 1 | tần suất recompute `heuristic_item_factor` (1 = mỗi gen) |
| `--exploit` | 0 | 1 = deposit best-so-far mỗi iteration thay vì iter-best (thử nghiệm) |

Build: `g++ -std=c++17 -O2 main.cpp -o main.exe`.

---

## 4. Hướng đi tiếp

1. **Đóng item pheromone** (đã chứng minh trơ/gated) — giữ code tinh khiết origin + refactor.
2. **initACO** (nhiều kiến gen-1) là thứ duy nhất có tín hiệu dương nhỏ trên unc/usw — xứng đáng đào tiếp.
3. **`--exploit` đang thử** (deposit best-so-far + rho↑) — nhưng exploitation chỉ thắng nếu có seed/basin tốt để bám.
4. **Sửa phép đo**: `termination_criterion()` dùng wall-clock → kết quả nhiễu theo tải CPU khi chạy song song.
   Cần chuyển sang fixed-số-generation hoặc CPU-time trước khi tin bất kỳ chênh lệch <1%.
