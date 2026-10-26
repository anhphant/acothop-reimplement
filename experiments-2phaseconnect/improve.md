# src-2p: drawbacks & ý tưởng cải thiện

Ký hiệu: `2p` = src-2p (có pheromone item), `origin` = src-origin (chỉ pheromone tour).
Bằng chứng từ `results_2p_origin_dsj1000/head_to_head.csv`: tổng thể 2p thua nhẹ (-0.10% mean),
nhưng **thua đậm tập trung ở các test lớn** (n=1000, cap 5–10):
`dsj1000_10_bsc_10_02` -2.57%, `dsj1000_10_bsc_10_01` -1.26%, `dsj1000_05_bsc_05_01` -1.23%,
`dsj1000_01_unc_10_01` -1.44%, `dsj1000_01_usw_05_02` -1.45%...
Instance cap=inf tie tuyệt đối → refactor trung thành, khác biệt 100% do pheromone item.

---

## 1. Drawbacks

### D1. Item-factor không chuẩn hóa, phá vỡ cân bằng tour
`src-2p/not-simple-2-tuneopt/pheromone/pheromone.cpp:110-122`

```cpp
heuristic_item[i]       = pow(sum_rw[i], phe_norm);          // sum_rw = Σ p/w, KHÔNG cận trên
heuristic_item_factor[i]= pow(heuristic_item[i], alpha_item); // alpha_item = 0.25
```

- `sum_rw` có thể cỡ 10³–10⁴ trên test lớn → factor nhân thẳng vào `total` (dòng 81)
  → át cả pheromone tour lẫn heuristic khoảng cách (vốn bị chặn bởi MMAS).
- Không scale-invariant: độ mạnh của item pheromone phụ thuộc độ lớn p/w của instance
  → càng tệ khi instance lớn / item giá trị cao (đúng pattern thua quan sát được).

### D2. Không có warm-up
`src-2p/not-simple-2-tuneopt/main.cpp:172-176`

Mọi item khởi tạo ở `item_trail_max` → `phe_norm = 1` với mọi thành phố → ngay vòng 1,
bias tĩnh Σp/w đã ở mức cực đại, tour bị bẻ cong trước khi pheromone tour kịp học gì.

### D3. Phản hồi chỉ một chiều → nhị phân hóa
`src-2p/not-simple-2-tuneopt/daemon_actions/optional.cpp:184-192`

- Deposit lên **mọi** item được chọn của lời giải nguồn mỗi iteration; item không chọn
  chỉ bay hơi chậm và bị kẹt ở `item_trail_min = max/(2n)` → `phe_norm ≈ 1/2000` → factor ≈ 1.
- Hệ quả: "được chọn = factor khổng lồ, không chọn = 1" — mất độ phân giải giữa các item trung bình.

### D4. Item pheromone rò sang pha 1 (tour construction)
`pheromone.cpp:60, 81-86` (`total_with_item_`, `city_factor_`, `fallback_score`)

Đáng lẽ item pheromone chỉ nên hướng dẫn packing (pha 2), nó lại bẻ cong cả quá trình
xây tour (pha 1) → mâu thuẫn với thiết kế 2-phase, phá chất lượng TSP thuần.

### D5. Vòng lặp feedback dương khóa cục bộ
Greedy packing (`packing.cpp`) phụ thuộc tour → item pheromone học theo packing greedy →
tour bị khóa theo item đó → packing lại lặp lại → lock. Trên test lớn (500 ants, ít iteration
vì mỗi gen đắt) không kịp thoát local optimum; restart MMAS cũng bị bias tĩnh kéo lại.

### D6. Một bộ tham số cho mọi class
`compare_2p_origin_dsj1000.py` chỉ dùng 1 bộ `rho_item=0.1, alpha_item=0.25` cho mọi
(items, ktype), trong khi tham số ACO được tune riêng từng class (`parameter_configurations`).

### D7. Nhỏ khác
- `compute_heuristic_item()` chạy **mỗi iteration** (`optional.cpp:199`) — O(nItems + nCities) + pow/city.
- `item_trail_min = item_trail_max/(2n)` → trên n=1000, độ phân giải phe_norm thô (2 mức biên là chính).

---

## 2. Hướng cải thiện

### I1. Chuẩn hóa item-factor (sửa gốc D1)
Thay `pow(sum_rw, phe_norm)` bằng dạng scale-invariant:
```cpp
heuristic_item[i] = exp(phe_norm * log1p(sum_rw[i]));   // hoặc
heuristic_item[i] = 1.0 + phe_norm * (normalized_topk_rw[i]);  // normalized ∈ [0,1]
```
- Đảm bảo factor nằm trong dải hẹp cố định (vd 1–5), không phụ thuộc độ lớn instance.
- Có thể tái dùng `city_profit_topk` (top-8 p/w đã có sẵn) thay cho tổng toàn phần.

### I2. Warm-up / schedule alpha_item (D2)
`alpha_item = 0` trong X% thời gian đầu (tour ổn định trước), sau đó tăng tuyến tính về giá trị
target. Biến thể: chỉ kích hoạt item-factor khi `sum_prob` candidate-list có phe_norm phân hóa đủ.

### I3. Phản hồi âm đối xứng (D3)
Mỗi lần deposit lên picked, đồng thời trừ một lượng nhỏ trên **unpicked** của lời giải nguồn
(giữ clamp min) → tương phản không bão hòa về 2 biên, item "suýt được chọn" giữ tín hiệu.

### I4. Chuyển item pheromone vào packing, không vào tour (D4, D5)
Giữ construction y nguyên origin (pha 1 tinh khiết); bơm phe_norm vào sort-key greedy packing:
```cpp
double key = delta*log_weight[j] + gamma*log_d_end[node] - theta*log_profit[j]
           - lambda_item * phe_item[j];          // λ_item ≥ 0
```
→ item pheromone chỉ làm packing tốt hơn, không phá TSP.

### I5. Tune rho_item/alpha_item (λ_item) theo class (D6)
Thêm vào `parameter_configurations` một bộ item-params riêng cho từng (items, ktype),
tune bằng irace/optuna (hạ tầng irace đã có sẵn trong `src-2p/not-simple-2-tuneopt/irace/`).

### I6. Giảm tần suất cập nhật (D7)
Tính `compute_heuristic_item()` mỗi 10–50 iteration thay vì mỗi vòng (item pheromone đổi chậm,
không cần cập nhật liên tục).

### I7. Trail-limits mượt hơn (D7)
Dùng `item_trail_min = p_conv * item_trail_max` kiểu MMAS tour (`exp(log(p_best)/nItems)`-based)
thay cho `max/(2n)` cố định, hoặc rank-based deposit (chỉ top-k item của source được deposit).

### I8. Ablation & kiểm chứng (bắt buộc kèm mọi thay đổi)
- `--alpha-item 0` (hoặc λ_item=0) phải **khớp origin từng bit** (đã có bằng chứng: tie trên cap=inf).
- Đo lại trên chính các instance thua đậm: `dsj1000_10_bsc_10_02`, `dsj1000_10_bsc_10_01`,
  `dsj1000_05_bsc_05_01`, `dsj1000_01_unc_10_01`, `dsj1000_01_usw_05_02`.
- Thứ tự thử: I1 (sửa gốc, nhỏ nhất) → I3/I4 → I2/I5 → I6/I7.
