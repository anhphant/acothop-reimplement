# Report — các hướng đã thử cải thiện ACO‑THOP và vì sao chưa hiệu quả

> Mục đích: ghi lại đầy đủ các hướng đã thử, kết quả đo, và lý do cơ chế vì sao từng hướng
> không hiệu quả — để không lặp lại và để khoanh đúng hướng còn lại.

---

## 0. Bối cảnh

**Bài toán THOP** (Traveling Thief Problem): kết hợp TSP (tour qua các thành phố) + knapsack
(nhặt item), liên kết qua tốc độ di chuyển:

```
speed = maxSpeed − nu · weight_so_far ,  nu = (maxSpeed − minSpeed) / capacity
objective = max Σ profit(picked)  ,  ràng buộc: Σ weight ≤ capacity  và  time ≤ maxTime
```

Định dạng `.thop` đang dùng: `capacity, maxTime, minSpeed, maxSpeed` — **không có rent rate**,
nên objective là **max profit dưới ràng buộc time budget**, không phải `profit − R·time`.

**Baseline `src-origin`**: ACO kiểu MMAS, chỉ có pheromone **tour**; tour heuristic = khoảng cách
(`1/(d+eps1)^α2`) × `city_heur_`; packing = greedy knapsack (θ/δ/γ ngẫu nhiên) + prune theo thời
gian + local search (2-opt/2.5-opt/3-opt).

---

## 1. Tổng kết một bảng

| # | Hướng | Kết quả đo | Vì sao fail (gốc) |
|---|-------|-----------|-------------------|
| 1 | Item pheromone 2-phase (bản exponent) | thua −0.10% mean trên 117 inst dsj1000 | factor không chuẩn hóa bẻ tour; phản hồi nhị phân |
| 2 | Fix item pheromone (linear + gate + warm-up + packing-bias) | ≈ control (trơ) | tín hiệu item tĩnh dư thừa, không chạm coupling |
| 3 | Tune `rho_item`/`alpha_item` | ±0.7% = nhiễu | tham số của 1 cơ chế đã trơ |
| 4 | `--item-heur-interval` (giảm tần suất cập nhật) | vô hiệu (bsc bị gate → y hệt nhau) | item vốn trơ + phép đo nhiễu wall-clock |
| 5 | `--exploit` (deposit best-so-far + rho↑) | thua 4/5 (−0.22% → −1.11%) | hội tụ sớm vào basin TSP thuần sai |
| 6 | initACO (nhiều kiến gen-1) | dương nhỏ unc/usw, âm bsc | chỉ cải seed, seed vẫn là basin TSP |
| 7 | Tour heuristic α3/α4 (profit/weight) | nhiễu, −1.81% trên bsc_10_02 | tín hiệu p/w tĩnh — cùng loại với item pheromone |

**Điểm chung của mọi hướng fail:** chúng bơm *giá trị item tĩnh* (p/w, profit, pick/no-pick),
nhưng khó khăn thật của THOP là *coupling động* weight→speed→time — chưa hướng nào chạm tới.

---

## 2. Chi tiết từng hướng

### 2.1 Item pheromone 2-phase (src-2p, bản exponent đầu)

**Làm gì:** thêm pheromone 1D cho từng item, học "item nào đáng lấy". Factor:
`heuristic_item(i) = pow(Σ p/w tại city i, phe_norm)`, nhân thẳng vào xác suất chọn thành phố.

**Kết quả** (`results_2p_origin_dsj1000`, 117 instance): 2p thua nhẹ **−0.10% mean**, thua đậm tập
trung instance lớn (vd `dsj1000_10_bsc_10_02` −2.57%).

**Vì sao fail:**
- `Σ p/w` không có cận trên → factor cỡ 10³–10⁴ át cả pheromone tour lẫn heuristic khoảng cách (D1).
- Mọi item khởi tạo ở max → vòng 1 đã bias cực đại, không có warm-up (D2).
- Deposit lên mọi item picked, item không pick chỉ bay hơi → tín hiệu nhị phân hóa (D3).
- Item pheromone rò sang pha xây tour, phá TSP thuần (D4).
- Feedback dương: tour→packing greedy→item pheromone→tour, khóa cục bộ (D5).

### 2.2 Fix item pheromone (linear + gate + warm-up + packing-bias)

**Làm gì** (`src-2p` hiện tại):
- Chuẩn hóa **linear blend**: `factor = 1 + α_item·phe_norm·10·(Σp/w / max)` ∈ [1, 1+10α].
- **Gate c3**: tương quan profit–weight `r > 0.5` → `α_item = 0` (bsc có r≈0.996 → bị tắt).
- **Warm-up**: 10% thời gian đầu `α_item=0`, hết warm-up chạy `packIterative` 1 lần.
- **Packing bias (I4)**: item pheromone đẩy sort-key packing `−α_item·bias`.
- `item_trail_min = max/√(2n)` ("nóng").

**Kết quả:**
- `results_ablate_alpha_item` (control/exponent/linear): linear ≈ control (chênh <0.4% = nhiễu).
- `results_ablate_decision` (36 inst items=01): `origin ≈ control ≈ linear` → item pheromone trơ.

**Vì sao fail:** tín hiệu "item nào ngon" đã có sẵn trong heuristic thành phố + packing greedy;
học lại ở mức item là **thừa**. Và nó không mã hóa **chi phí thời gian** của item nặng.

### 2.3 Tune `rho_item` / `alpha_item`

**Kết quả** (`tune_rhoitem_alphaitem.csv`, 30 điểm trên dsj1000_10): mọi `α_item>0` chỉ ±0.7% quanh
baseline (`α_item=0`), điểm tốt nhất +0.76% — nhiễu, không có tín hiệu.

**Vì sao fail:** đây là tune tham số của một cơ chế đã trơ (mục 2.2).

### 2.4 `--item-heur-interval` (I6 — giảm tần suất cập nhật item heuristic)

**Làm gì:** `compute_heuristic_item()` chạy mỗi N gen thay vì mỗi gen.

**Kết quả** (`results_ablate_item_heur_interval`): i1/i5/i10/i50 ≈ nhau; trên bsc **giống hệt từng
chữ số** (vì gate đã tắt item). unc/usw chênh ±0.4% = nhiễu.

**Vì sao fail:** item đã trơ, và — quan trọng hơn — phát hiện **phép đo nhiễu wall-clock** (mục 4).

### 2.5 `--exploit` (deposit best-so-far thay vì iter-best + rho↑)

**Làm gì:** thêm flag `--exploit` để deposit luôn best-so-far (elitist) thay vì iter-best, kèm rho×1.3.

**Kết quả** (`results_ablate_exploit`, 5 instance thua cũ): **thua 4/5**:
`unc_10_01 −0.87%`, `05_bsc_05_01 −0.22%`, `10_bsc_10_01 −0.80%`, `10_bsc_10_02 −1.11%`.

**Vì sao fail:** xác nhận chẩn đoán hội tụ sớm — solver đang over-exploit vào basin **TSP thuần**
(vì α3=α4=0, tour không biết item). Exploitation mạnh hơn = khóa chặt hơn vào basin sai đó.

### 2.6 initACO (`--init-ants`: nhiều kiến ở generation 1)

**Làm gì:** gen 1 dùng N kiến (thay vì `number_of_ants`), sau đó về bình thường.

**Kết quả:**
- a280: bsc +0.03%, unc −0.13%, usw +0.12%.
- dsj1000_10: bsc **−0.41%**, unc **+0.38%**, usw **+0.20%**.

**Vì sao chỉ một phần:** bản chất là *multi-start miễn phí* đúng lúc pheromone còn uniform — cải
thiện seed bằng cách lấy max của nhiều mẫu heuristic. Nhưng gain ~`σ√(2 ln N)` bão hòa log, và seed
vẫn là basin TSP (tour không biết item) → chỉ giúp nhẹ, không phải đòn bẩy.

### 2.7 Tour heuristic α3/α4 (profit/weight)

**Phát hiện quan trọng:** mọi script dsj1000 (`compare_2p_origin_dsj1000.py`, `compare_initant_dsj1000.py`)
đều **không truyền α3/α4/eps1/eps2** → dùng mặc định `α3 = α4 = 0`. Vậy tour construction trong
toàn bộ suite dsj1000 là **TSP thuần theo khoảng cách**, không hề có thông tin profit/weight/item.

**Thử bật lên** (`results_ablate_tour_heur`, giá trị từ `note.txt`: α3=1.36, α4=0.25, eps1=125.85,
eps2=24948.57): `unc +0.08%`, `usw −0.18%`, `05_bsc +0.08%`, `10_bsc_01 +0.02%`, `10_bsc_02 −1.81%`.

**Vì sao fail:** p/w là **tín hiệu item tĩnh** — cùng loại với item pheromone. Trên bsc p/w ≈ hằng
nên vô tín hiệu (thậm chí hại); trên unc/usw nó thừa với thông tin profit đã có. Không chạm coupling.

---

## 3. Phát hiện gốc rễ (quan trọng nhất)

1. **Tour construction mù hoàn toàn thông tin item** trong mọi experiment dsj1000 (α3=α4=0).
2. **Objective & coupling thật:** max profit dưới ràng buộc `time ≤ maxTime`, với `speed` giảm theo
   trọng lượng (`maxSpeed 1.0 → minSpeed 0.10` khi đầy balo = chậm 10×). `maxTime ≈ 1.6e7` trong khi
   tour đủ 1000 thành phố cần ~`3e7` → **time budget bị chặn**, phải *chọn* thành phố đáng ghé.
3. **Packing đã có sẵn đúng công thức coupling** nhưng tour không dùng:
   ```cpp
   // packing.cpp:214
   double min_dt = geom_detour * inv_vmax + (w * dist_dest) * inv_vmax_sq;
   //                ↑ thời gian detour        ↑ thời gian chậm do trọng lượng
   ```
   Công thức này nói rõ: giá trị thật của một item = `profit − (detour_time + slowdown_time)`.
   Item pheromone và heuristic p/w đều **không có vế trừ này** — nên chúng học sai đại lượng.

**Kết luận gốc rễ:** mọi hướng fail vì đều bơm *giá trị item tĩnh* (profit/p/w/pick) vào tour;
thứ đang thiếu là **net-value theo thời gian** — chi phí detour + chậm do trọng lượng so với profit.
Chưa hướng nào đưa `min_dt`-aware value vào tour construction.

---

## 4. Vấn đề phép đo (wall-clock)

`termination_criterion()` dùng **wall-clock** (`chrono::steady_clock`). Khi chạy song song (8 jobs),
số generation hoàn thành trong cùng time-limit **thay đổi theo tải CPU** → kết quả nhiễu.

**Bằng chứng trực tiếp:** instance `05_bsc_05_01` bị gate tắt item (α_item=0, r=0.996) → về thuật
toán `interval` không thể tác động, nhưng chạy 2 batch khác nhau ra **2 600 069 vs 2 604 941**
(cùng seed 269070: 2591620 vs 2605067). → mọi chênh lệch <~1% trong các bảng trên đều phải coi là
nhiễu trừ khi đo lại serial (jobs=1) hoặc chuyển sang fixed-số-generation / CPU-time.

---

## 5. Hướng chưa thử (đề xuất theo thứ tự)

1. **Kiểm tra time budget có thực sự binding không** — nếu packing bị chặn bởi capacity chứ không
   phải time, thì coupling là thứ yếu. (Probe: `totalWeight < capacity` mà packing vẫn bỏ item vì
   hết giờ → time binding.)
2. **Net-value theo thời gian cho tour** (nếu time binding):
   ```
   net(city j) = Σ profit(items@j) − λ·(detour_time(j) + Σ weight·dist_remaining·nu/maxSpeed²)
   ```
   bơm vào `city_heur_` (tái dùng đúng công thức `min_dt` đã có). Đây là "item info" đúng chỗ.
3. **Sửa phép đo** trước khi tin bất kỳ chênh lệch <1%: fixed-gen hoặc CPU-time.
4. (Tùy chọn) tune lại α3/α4/eps1/eps2 **đúng quy trình** (per-class, bằng irace/optuna) — vì thử
   ở mục 2.7 chỉ là 1 điểm chưa tune, không phải bằng chứng dứt khoát rằng heuristic p/w vô dụng.

---

## Phụ lục — file kết quả & script

| Kết quả | File |
|---|---|
| 2p vs origin (117 inst) | `results_2p_origin_dsj1000/{raw_results,summary,head_to_head}.csv` |
| 5 instance thua đậm | `results_2p_5lose/head_to_head.csv` |
| control/exponent/linear | `results_ablate_alpha_item/group_01/raw.csv` |
| origin/control/linear | `results_ablate_decision/{fp_subset,all01}/raw.csv` |
| tune rho/alpha item | `tune_rhoitem_alphaitem.csv` |
| item-heur-interval | `results_ablate_item_heur_interval{,_i5}/raw.csv` |
| exploit | `results_ablate_exploit/raw.csv` |
| initACO | `results_initant{,dsj1000}/head_to_head.csv` |
| tour heuristic α3/α4 | `results_ablate_tour_heur/raw.csv` |

Script tái chạy: `ablate_alpha_item.py`, `ablate_decision.py`, `ablate_item_heur_interval.py`,
`ablate_exploit.py`, `ablate_tour_heur.py`, `compare_initant{,dsj1000}.py`, `tune_rhoitem_alphaitem.py`.
