# Implementation plan — src-2p: I1 + warm-up (PackIterative)

Mục tiêu 2 thay đổi trên `src-2p/not-simple-2-tuneopt/` — **không thêm tham số mới**, chỉ dùng `alpha_item` có sẵn:

1. **I1** — chuẩn hóa item-factor (hết phụ thuộc scale instance)
2. **Warm-up** — chạy ACO thường (alpha_item=0) lấy tour tốt → `PackIterative` packing (chưa dùng pheromone item) → seed pheromone item ban đầu → mới bật pheromone item

---

## 1. I1 — chuẩn hóa item-factor (chỉ alpha_item)

**File**: `pheromone/pheromone.cpp`, hàm `compute_heuristic_item()` (dòng 89–128)

**Hiện tại** (không chuẩn hóa):
```cpp
heuristic_item[i] = pow(sum_rw[i], phe_norm);   // sum_rw cỡ 10^3–10^4 → factor không kiểm soát
```

**Sửa thành** (scale-invariant):
```cpp
// sau vòng tính sum_rw/sum_phe/item_count:
double max_rw = 1.0;
for (long i = 0; i < nCities; ++i)
    if (item_count[i] > 0) max_rw = max(max_rw, sum_rw[i]);

for (long i = 0; i < nCities; ++i) {
    if (item_count[i] > 0 && sum_rw[i] > 0.0) {
        double norm     = sum_rw[i] / max_rw;          // (0,1] — chuẩn hóa theo instance
        double phe_norm = avg_phe / item_trail_max;    // [0,1]
        heuristic_item[i] = pow(1.0 + norm, phe_norm); // ∈ [1,2]
    }
}
// phần còn lại (fallback, pow(..., alpha_item)) GIỮ NGUYÊN
```
- Factor cuối ∈ **[1, 2^alpha_item]** — bị chặn chặt, không phụ thuộc scale instance.
- **Không thêm param**: biên độ gộp luôn vào `alpha_item` hiện có (alpha_item=0.25 → factor max 1.19).
- `alpha_item=0` → factor=1.0 → khớp origin từng bit (regression giữ nguyên).

---

## 2. Warm-up + seed bằng PackIterative

### 2.1 Port `packIterative` vào src-2p

**Nguồn**: `.claude/worktrees/1908fix-opt/src/current-best-algo/packing/pack_iterative.h` (đã có sẵn).

**File mới**: `packing/pack_iterative.cpp` trong src-2p — chỉ 2 thay đổi adapter:
- `itemsAtCity[city]` (vector<vector>) → CSR hiện có: `for (int idx = cityItemStart[city]; idx < cityItemStart[city+1]; ++idx) item = itemsByCity[idx]`.
- `Packing.picked` đang là `vector<char>` → `assign(nItems, 0)`, kiểm tra `picked[x]` như cũ.

Include sau `packing/packing.cpp` (cần `Packing`, `nu`, `maxTime`...).

> ponytail: vòng khởi tạo priority-queue là O(nItems × cost(computeTime)) — trên dsj1000_10 (10k items) có thể mất ~phút. Nếu vượt budget: chỉ đưa top-K item theo p/w vào PQ (K ≈ 5·nCities) — nâng cấp khi đo thấy chậm.

### 2.2 Tích hợp vào vòng lặp chính

**File**: `main.cpp` (vòng `while(!termination_criterion())`, dòng 187–202) + `pheromone/pheromone.cpp`

Trạng thái mới (global trong `pheromone.cpp`):
```cpp
bool warmup_done = false;
```

Trong vòng lặp (`WARMUP_FRAC` là hằng nội bộ — không cần flag CLI):
```cpp
const double WARMUP_FRAC = 0.10;                     // phần thời gian warm-up (đặt TRƯỚC vòng lặp)
const double SAVED_ALPHA_ITEM = params.alpha_item;   // lưu 1 lần, TRƯỚC vòng lặp

bool warmup = (elapsed_time() < params.time_limit * WARMUP_FRAC);
params.alpha_item = warmup ? 0.0 : SAVED_ALPHA_ITEM; // tắt item trong warm-up, khôi phục sau đó

if (!warmup_done && !warmup) {            // hết warm-up: chạy PackIterative 1 lần
    Packing p0 = packIterative(Optional.best_solution.sequence);  // KHÔNG dùng pheromone item
    if (p0.totalProfit > Optional.best_solution.packing.totalProfit) {
        Optional.best_solution.packing = p0;
        if (p0.totalProfit > Optional.best_cost) Optional.best_cost = p0.totalProfit;
    }
    Pheromone.seed_item_pheromone(Optional.best_solution.packing);
    warmup_done = true;
}
```
`seed_item_pheromone` (thêm vào `Pheromone`):
```cpp
void seed_item_pheromone(const Packing& p0) {
    for (long k = 0; k < nItems; ++k)
        phe_item[k] = p0.picked[k] ? item_trail_max : item_trail_min;
    compute_heuristic_item();
}
```
- `init_item_pheromone()` ở `main.cpp:172-176` **GIỮ NGUYÊN** (vô hại: alpha_item=0 trong warm-up; sẽ bị seed ghi đè).
- `WARMUP_FRAC=0` → seed chạy ngay iteration 1 (tương đương hành vi cũ + init thông minh hơn).

### 2.3 Điểm cần giữ nguyên (regression)

- `precompute_total_with_item()` (pheromone.cpp:77) không đổi; alpha_item=0 → factor=1 → khớp origin.
- `optional.cpp` deposit/evaporate item không đổi (trong warm-up không ảnh hưởng gì vì alpha_item=0; sau đó seed ghi đè).
- `State::solution_cost()` → `pack()` không đổi.

---

## 3. Thứ tự triển khai & kiểm chứng

1. **Bước A — I1** (1 file, 0 param mới): `--alpha-item 0` phải khớp origin từng bit (`check_regression.ps1` / 1 instance cap=inf).
2. **Bước B — warm-up + packIterative** (pack_iterative.cpp + main.cpp + pheromone.cpp): `--alpha-item 0` vẫn khớp origin; log thời gian packIterative lúc hết warm-up (nếu > vài % budget → top-K ponytail mục 2.1).
3. **Đo đối chiếu** trên 5 instance thua đậm: `dsj1000_10_bsc_10_02`, `dsj1000_10_bsc_10_01`, `dsj1000_05_bsc_05_01`, `dsj1000_01_unc_10_01`, `dsj1000_01_usw_05_02` — so sánh origin / 2p-cũ / 2p-mới.
4. **Sweep** `alpha_item ∈ {0.1, 0.25, 0.5, 1.0}` trên 1–2 instance rồi tune đầy đủ bằng irace (đã có `irace/` sẵn).

**Rủi ro chính**: packIterative chậm trên 10k items (mục 2.1 ponytail); factor bị chặn [1, 2^alpha] có thể yếu hơn bản cũ.
