# Metric — đánh giá & so sánh 2 solver THOP

> Dùng để so sánh 2 solver stochastic (src-2p vs src-origin, hoặc baseline vs cải tiến)
> trên bộ instance dsj1000. Mọi metric bên dưới giả định **paired cùng seed** (seed
> giống nhau ở cả 2 solver để khử nhiễu RNG).

---

## 0. Dữ liệu đầu vào

Mỗi run ghi 1 dòng `raw.csv`:

```
instance, solver, seed, profit, runtime, status
```

Trước khi so, **phải** đảm bảo phép đo sạch (xem mục 4) — nếu không mọi metric đều nhiễu.

---

## 1. Chất lượng lời giải

| Metric | Công thức | Ý nghĩa |
|---|---|---|
| **mean** | `Σ profit / n_seeds` | chất lượng trung bình |
| **best** | `max(profit)` | tốt nhất tìm được |
| **worst** | `min(profit)` | tệ nhất (độ rủi ro) |
| **std** | độ lệch chuẩn qua seeds | độ phân tán |
| **CV** | `std / mean` | độ bất ổn chuẩn hóa (so được giữa instance khác scale) |
| **gap % so best-known** | `(best_known − profit) / best_known` | độ lệch so tham chiếu; trung bình qua instances |
| **mean_pct (paired)** | `mean((B_seed − A_seed) / A_seed) × 100` | % cải thiện trung bình so baseline |

> **best-known**: nếu không có giá trị tối ưu chuẩn, lấy `max` trên **tất cả** solver +
> tất cả seed làm tham chiếu (best-found). gap% là metric chuẩn trong paper THOP/TTP.

---

## 2. Kiểm định thống kê (quan trọng nhất)

Phân biệt "tốt hơn thật" vs "tốt hơn do nhiễu".

### 2.1 Wilcoxon signed-rank (so 2 solver)

- **Khi dùng:** so sánh đúng 2 solver — đây là chuẩn cho metaheuristics.
- **Dữ liệu:** với mỗi instance, lấy `mean_profit` của từng solver (paired) → 2 dãy `x, y`.
- **Giả thuyết:** H0 = "2 solver như nhau".
- **Kết quả:** p-value < 0.05 → khác biệt có ý nghĩa; kèm **effect size** = median của `y − x` (hoặc `(y−x)/x`).

```python
from scipy.stats import wilcoxon
w, p = wilcoxon(y, x)   # y = cải tiến, x = baseline (paired)
```

### 2.2 Friedman + Nemenyi posthoc (so ≥ 3 solver)

- **Khi dùng:** 3 solver trở lên (vd origin / 2p-exponent / 2p-linear).
- **Dữ liệu:** rank từng instance theo mean profit (1 = tốt nhất), rồi trung bình rank mỗi solver.
- Friedman test cho "có solver nào khác nhau"; Nemenyi cho "cặp nào khác nhau".

```python
from scipy.stats import friedmanchisquare
stat, p = friedmanchisquare(a_mean, b_mean, c_mean)   # 1 dãy per solver, aligned theo instance
```

### 2.3 Mô tả (không phải test, nhưng dễ báo cáo)

- **win/loss/tie** (paired per seed): đếm `B_seed > A_seed` / `<` / `=`.
- **average rank**: rank solver mỗi instance → trung bình.

> win/loss chỉ là mô tả; phải kèm Wilcoxon mới kết luận được.

---

## 3. Runtime / hội tụ

| Metric | Nguồn | Ý nghĩa |
|---|---|---|
| **runtime_mean** | `runtime` (log) | thời gian chạy trung bình |
| **time-to-best** | dòng `found at time` | solver nào đạt best sớm hơn → hội tụ nhanh |
| **best-vs-time curve** | log `best ... time` | vẽ đường hội tụ (cho paper) |
| **#generations** | log `generation` | nếu đổi sang fixed-gen để đo sạch |

---

## 4. Vấn đề phép đo wall-clock (bắt buộc đọc)

`termination_criterion()` đang dùng **wall-clock** (`chrono::steady_clock`). Khi chạy song song
(8 jobs), số generation hoàn thành trong cùng time-limit **thay đổi theo tải CPU** → cùng seed
chạy 2 lần ra profit khác nhau.

**Hệ quả:** mean/std/gap/Wilcoxon đều nhiễu; chênh lệch <~1% không thể kết luận. Với 3 seeds,
Wilcoxon gần như chắc chắn `p > 0.05` cho mọi khác biệt đang thấy.

**Sửa:** chạy `--jobs 1` (serial) hoặc đổi termination sang **fixed số generation** / CPU-time,
rồi đo lại với **10–30 seeds** (3 seeds quá yếu về thống kê).

---

## 5. Quy trình so sánh chuẩn

1. Sửa phép đo (fixed-gen / serial).
2. Chạy 2 solver, cùng seed, **10–30 seeds** / instance.
3. Gom `mean_profit` mỗi (instance, solver).
4. Tính: mean, std, CV, gap%, mean_pct (paired).
5. Wilcoxon (2 solver) hoặc Friedman+Nemenyi (≥3).
6. Báo cáo: bảng tổng + p-value + effect size + average rank.

---

## 6. Đầu ra mong muốn (một bảng)

| instance | A_mean | B_mean | B−A | pct% | gap_A% | gap_B% | win-loss-tie |
|---|---|---|---|---|---|---|---|
| ... | ... | ... | ... | ... | ... | ... | ... |

Cuối bảng: **Tổng** — median diff%, Wilcoxon p-value, win/loss/tie, average rank.

---

*Công cụ gợi ý: `stats_compare.py` đọc 2 file `raw.csv` (paired seed) → in toàn bộ bảng trên
(Wilcoxon/Friedman qua `scipy.stats`).*
