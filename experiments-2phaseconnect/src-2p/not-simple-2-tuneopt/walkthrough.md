# Walkthrough — sửa solver chạy được trên `pla33810`

Mục tiêu: solver ACO cho THOP/TTP đang chết khi chạy instance `pla33810` (33810 thành phố,
33808 item). Sửa để nó **load + chạy + ghi output không crash/OOM**, đồng thời **giữ nguyên
kết quả** trên các instance nhỏ (regression `sample_instance.txt` → objective 25, items `[1,2,3]`).

---

## 1. Nguyên nhân gốc (3 tầng)

Đọc code cho thấy solver đang dùng mô hình bộ nhớ **dense O(n²)** + cap static cố định:

1. **Cap cứng** — `helper/helper.cpp` khai báo `MAX_CITIES = 4000`. `pla33810` cần 33810,
   nên `assert(n <= MAX_CITIES)` nổ và mọi mảng `[MAX_CITIES]` bị tràn khi đọc node thứ 4000+.

2. **Ma trận dense O(n²)** — blocker thật sự, không phải chỉ `MAX_CITIES`:
   - `dist_memo[MAX_CITIES][MAX_CITIES]` (int) → 4.6 GB cho n=33810.
   - `heuristic` / `phe` / `total[MAX_CITIES][MAX_CITIES]` (double ×3) → 27 GB.
   - `ls.cpp compute_local_distances` cấp ma trận `n×n` mỗi lần gọi LS → ~9 GB/call.
   Tổng ~32 GB, vượt quá ~16 GB RAM ⇒ bump `MAX_CITIES` không cứu được.

3. **Static-array phình do cap** — `pre_seq[MAX_ANTS][MAX_CITIES]`, `Ant ants[MAX_ANTS]`
   (mỗi `Ant` mang `sequence[4000]` + `visited[4000]` + `picked[100000]` ≈ 120 KB).

## 2. Chiến lược

Chuyển từ **dense O(n²)** sang **candidate-list O(n·k)** (k = `NN_ANTS` ≈ 20 — cơ chế
`knn[][]` đã có sẵn), và cấp phát mọi mảng theo **kích thước runtime** (`std::vector` / heap)
thay vì cap static. Bộ nhớ giảm từ ~32 GB xuống ~50 MB + ants ≈ 100 MB.

Mấu chốt bảo toàn hành vi: với instance **nhỏ** (K bao phủ hết mọi láng giềng, ví dụ
sample n=4 ⇒ K=3), sparse == dense **bit-for-bit** ⇒ regression giữ nguyên. Với instance
lớn (n > K), các arc ngoài candidate-list bị xấp xỉ bằng `trail_0` — đây là chuẩn MMAS
candidate-list (Dorigo & Stützle).

## 3. Thay đổi theo file

| File | Thay đổi |
|---|---|
| `helper/helper.cpp` | Xóa 4 hằng `MAX_CITIES/MAX_ITEMS/MAX_ANTS/MAX_KN`. |
| `problem/thopproblem.cpp` | `nodes/items/city_profit_topk/city_weight_topk` → `vector`. Xóa `dist_memo` + `precalculate_distances` cũ; thay bằng `distance(int,int)` tính on-the-fly (sqrt+ceil) và giữ đúng logic node dummy (far với mọi thành phố trừ start/dest). `compute_upper_bound`, `compute_city_topk`, `readData` → vector. |
| `packing/packing.cpp` | `Packing.picked` → `vector<char>`; `itemsByCity/cityItemStart/cityIdToNodeIdx` → vector; toàn bộ buffer static → vector resize theo `nCities`/`nItems`; `sort`/`memcpy` → `.begin()`/`.data()`. |
| `aco.cpp` | `componentList` → `vector<Component>`; `knn` → `vector<vector<int>>`; `State.sequence` → `vector<int>`; bỏ các `assert(<= MAX_*)`; `init_from_nn_tour`/`ACOinit` → vector; `Colony.ants.assign(...)`. |
| `colony/ant.cpp` | `Memory.visited_components` → `vector<char>`; thêm `transition_score_slot` (O(1) cho candidate); `move_next`/`neighbour_choose_best_next` dùng slot; `initialize` pre-size `state.sequence`. |
| `colony/colony.cpp` | `Ant ants[]` → `vector<Ant>`; **fix off-by-one**: `initialize_ants` truyền `nCities + 1` (slot dummy). |
| `pheromone/pheromone.cpp` | **Rewrite hoàn toàn** sang sparse: `phe_/total_/heuristic_` là `vector<double>` kích thước `n*K`, index theo slot trong `knn`. `getTotal/getPheromone` giữ signature (i,j) bằng cách lookup slot, fallback on-the-fly cho arc ngoài list. Thêm `getTotal_slot` cho hot path. |
| `daemon_actions/optional.cpp` | `pre_seq/pre_ls_cost/pre_seq_size` → vector; `ls_run`'s `tour` → vector. |
| `main.cpp` | `tour/output_cities` (phần ghi output) → vector. |
| `ls/ls.cpp` | Xóa `compute_local_distances` (dense n×n). `compute_local_nn_lists` nhận `original_tour` và tính distance on-the-fly. Thêm macro `LS_DIST(a,b)` thay cho `distance[a][b]`. Đảo thứ tự cấp `original_tour` trước, bỏ `free(distance)`. |

## 4. Hai bug tiềm ẩn đã lộ khi đổi array → vector

Khi chuyển từ mảng static (kích thước cố định, không check biên) sang `vector` (check biên /
realloc), hai lỗi tiềm ẩn cũ bị lộ ra và đã sửa:

1. **Off-by-one ở dummy** (`colony/colony.cpp`): `Ant::initialize` gọi `memory.visit(nCities)`
   (đánh dấu dummy), nhưng `initialize_ants` chỉ reset `nCities` slot ⇒ ghi index `nCities`
   ra ngoài biên vector. Fix: truyền `nCities + 1`.

2. **`State.sequence` chưa được cấp trước** (`colony/ant.cpp`): `visit_component` dùng
   `sequence[sequenceSize++] = id` (index, không phải `push_back`). Mảng cũ luôn đủ chỗ,
   vector rỗng thì segfault. Fix: `initialize` pre-size `sequence` = `numberOfComponents`.

## 4b. Hai bug OOB do vector thu hẹp so với mảng static

Mảng static `[MAX_CITIES]` trả 0 ở slot không được khởi tạo (dummy), vector thì không — khi
chạy instance lớn, `-D_GLIBCXX_DEBUG` bắt được 2 chỗ đọc quá biên:

1. **`city_weight_topk[nCities]` / `city_profit_topk[nCities]`** (`heuristic_ij`, pheromone):
   dummy node (id == nCities) được đọc như một thành phố, nhưng hai vector này chỉ size
   `nCities`. Fix: `assign(nCities + 1, 0.0)` (slot dummy = 0, giống static zero-init).

2. **`city_to_pos[...]` trong `compute_local_nn_lists`** (ls.cpp): tour **bị ngắt giữa chừng**
   bởi time-limit (partial tour, `sequenceSize < nCities`) thì `n = sequenceSize+1 < nCities+1`,
   nhưng `knn[city][i]` trả city-id tới `nCities`. Fix: `city_to_pos` size `nCities+1` (không
   phải `n`), lọc city không thuộc tour bằng `-1`.

## 5. Kiểm chứng

- **Regression (bit-exact)**: `sample_instance.txt` vẫn cho
  `Best objective: 25.0000000000` và items `[1, 2, 3]` — giống trước refactor.
- **`pla33810` (LS off)**: load 33810 cities / 33808 items, 2 generations, ghi solution
  `cost 139114`, exit 0, không crash/OOM.
- **`pla33810` (LS on)**: load + 2 generations + ghi solution, exit 0.
- **`dsj1000` (LS on, `-D_GLIBCXX_DEBUG`)**: 512 generations không có OOB/assert.

> Ghi chú harness: `check_regression.ps1` hiện fail trong PowerShell 5.1 vì native stderr
> (`log_debug`) + `$ErrorActionPreference = "Stop"` bị bọc thành `NativeCommandError`, trước
> cả bước assert. Đây là quirk của PS 5.1, **không** liên quan refactor; assert của script
> (objective 25 / `[1,2,3]`) đã được xác nhận pass bằng cách chạy thủ công.

## 6. Perf: reuse `knn` global cho local search (đã sửa)

Triệu chứng ban đầu: mỗi generation với LS on tốn ~305s (log người dùng). Nguyên nhân:
`compute_local_nn_lists` (ls.cpp) là **O(n² log n)** — với mỗi trong 33811 thành phố, nó tính
và **full-sort** khoảng cách tới tất cả 33811 thành phố. Hàm này chạy **một lần cho mỗi ant
trong mỗi generation** (bên trong `two_opt_first`), nên generation ≈ `số ant × ~80s`.

Fix: tour của LS chứa **toàn bộ** thành phố (+ dummy) — danh sách láng giềng của nó **giống
hệt** `knn` global đã tính một lần trong `ACOinit`. Chỉ cần ánh xạ city-id → vị trí tour thay
vì sort lại:

- `aco.cpp`: build `knn` với width `KNN_WIDTH = max(nn_ants, nn_ls)` (đủ cho cả construction
  lẫn LS); `NN_ANTS` giữ nguyên cho construction/pheromone.
- `ls.cpp`: `compute_local_nn_lists` giờ chỉ remap `knn` global + lọc city ngoài tour
  (O(n·k) thay vì O(n² log n)). Xóa `ls_sort2`/`ls_swap2` (dead code).

Kết quả: LS mỗi ant ~80s → ~0.2s; generation 20 ants ~5s (trước ~nhiều phút).

## 7. Giới hạn & việc tiếp theo (chưa làm — không phải blocker)

- **Perf khởi tạo O(n² log n) vẫn còn 1 chỗ**: build `knn` global (~80s cho n=33810, một lần
  duy nhất). `log(n)≈15 ≈ k=20` nên partial-sort không nhanh hơn; hướng đi là k-d tree nếu
  cần khởi tạo nhanh hơn.
- **Xấp xỉ arc ngoài candidate-list** (n > K ≈ 20): pheromone của arc không nằm trong `knn`
  bị bỏ qua / xấp xỉ `trail_0`. Nếu chất lượng giải trên instance lớn tụt, nâng cấp bằng
  hashmap lưu pheromone off-list.

## 8. Tối ưu tốc độ generation (đơn luồng)

Profile cho thấy **tour construction, không phải LS, mới là bottleneck** (~2.4× update).
Hai chi phí ẩn chính và cách sửa:

1. **`::distance` làm string-search mỗi lần gọi** (`edgeWeightType.find("CEIL_2D")` ~1.5 tỷ
   lần) → thay bằng `bool edge_is_ceil` tính một lần trong `readData` (bit-exact).
2. **`choose_best_next` (fallback full-scan)** gọi ~800 lần/ant; mỗi lần quét n thành phố và
   làm `slot_of` (O(K)) + `pow(trail_0,α1)` + 3 `pow` cho mỗi thành phố chưa thăm. Sửa:
   - Trong fallback mọi candidate đều đã thăm ⇒ bỏ `slot_of` và hằng số `pow(trail_0,α1)`
     (không ảnh hưởng argmax) → `Pheromone.fallback_score(i,j)`.
   - Precompute `city_heur_[j] = profit_factor·weight_factor` (1 lần) và `dist_pow_[d] =
     pow(1/(d+eps1),α2)` (lookup theo khoảng cách nguyên) → fallback chỉ còn **sqrt + 2 read**.

Các bước khác (theo plan):

3. **LS candidate distance** (`dist_city[i][k]` + `nn_dist`): bỏ `sqrt` khỏi vòng quét
   candidate 2-opt/2.5-opt/3-opt. (Đồng thời sửa bug `local_nn_ls` không khởi tạo trong
   `two_h_opt_first`/`three_opt_first`.)
4. **Packing log-sort-key**: `pow(profit,θ)`/`pow(weight,δ)`/`pow(d_end,γ)` → sort key
   `δ·log_weight − θ·log_profit + γ·log_d_end` (đơn điệu, đổi nhẹ FP).
5. **Tour micro**: precompute `total_with_item_` (1 lần/generation, bỏ phép nhân trong
   `getTotal_slot`), bỏ `prob_ptr.assign` mỗi bước.

**Kết quả đo (pla33810, 20 ants, LS on):** generation ~5.3s → ~2.4s; riêng tour construction
5.99s → ~1.2s. Chi phí còn lại chủ yếu là `sqrt` (fallback + `radius` LS) — bất khả giảm
khi không có spatial index.

---
*Build*: `g++ -std=c++17 -O2 main.cpp -o main.exe` (single translation unit — mọi `.cpp`
được `#include` vào `main.cpp`). Chạy qua `run.ps1` (compile lại mỗi lần).
