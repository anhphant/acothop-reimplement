# LKH-3 — Hướng dẫn chi tiết

> Lin–Kernighan–Helsgaun version 3 (Keld Helsgaun, 2017) — giải các bài toán định tuyến có ràng buộc: **CVRP, VRPTW**, MTSP, ...

---

## 1. Bối cảnh & lịch sử

- **LKH-2** (Helsgaun, 2000): một trong những heuristic mạnh nhất cho **TSP**.
  Hai ý tưởng làm nên sức mạnh:
  1. **Candidate set** (tập cạnh ứng viên): với mỗi nút chỉ giữ lại một số ít cạnh "hứa hẹn", đo bằng **α-nearness** (dựa trên cận dưới 1-tree). Move chỉ duyệt trên các cạnh ứng viên → tìm kiếm nhanh hơn hàng chục lần so với k-opt thông thường.
  2. **Sequential k-opt move**: thay vì áp dụng từng 2-opt/3-opt rời rạc, LKH ghép nhiều phép trao đổi 2-opt thành một *move tuần tự* có gain dương.

- **LKH-3** (Helsgaun, 2017): mở rộng LKH-2 từ TSP sang bài toán **có ràng buộc** (dung lượng, time window, ...). Ý tưởng then chốt: **giữ nguyên cỗ máy tìm kiếm của LKH-2, chỉ đổi hàm mục tiêu thành hàm có phạt (penalty)**.

- Bản LKH-3 mới (3.0.x) bổ sung thêm cơ chế **quần thể (population-based / GA)** để tăng tính đa dạng trên instance lớn, nhưng phần lõi vẫn là penalty + sequential k-opt.

---

## 2. Phát biểu bài toán

### 2.1 CVRP (Capacitated VRP)

- Tập nút `V = {0} ∪ N`, trong đó `0` là **depot**, `N` là `n` khách hàng.
- Mỗi khách `i` có nhu cầu `q_i ≥ 0`.
- Một xe có **dung lượng** `Q` (số xe thường được xem là không giới hạn hoặc cố định `K`).
- Cần tìm tập tuyến (route), mỗi tuyến xuất phát & kết thúc tại depot, mỗi khách được thăm đúng một lần, tổng nhu cầu trên mỗi tuyến `≤ Q`, và **tổng quãng đường nhỏ nhất**.

### 2.2 VRPTW (thêm Time Window)

- Mỗi khách `i` có cửa sổ thời gian `[a_i, b_i]` và thời gian phục vụ `s_i`.
- Phải đến trong cửa sổ `[a_i, b_i]` (tùy biến thể: cho phép đợi nếu đến sớm).

### 2.3 Biểu diễn nghiệm

Một nghiệm là **tập các tuyến**; mỗi tuyến là danh sách nút bắt đầu và kết thúc bằng depot:

```
[0, 3, 5, 2, 0]   # một tuyến
```

Số tuyến = số xe sử dụng (có thể cố định hoặc thả nổi, có phạt cho mỗi xe).

---

## 3. Từ LKH-2 đến LKH-3: ý tưởng trung tâm

LKH-2 tối ưu `dist(S)` trên không gian nghiệm **khả thi** (đúng là một chu trình TSP). Với CVRP/VRPTW, không gian khả thi **không liên thông tốt** dưới các move cục bộ: một move nhỏ (chuyển 1 khách) dễ làm vỡ ràng buộc dung lượng/thời gian, khiến tìm kiếm mắc kẹt.

**Giải pháp của LKH-3:** *bỏ* ràng buộc khỏi không gian tìm kiếm, biến nó thành **số hạng phạt** trong hàm mục tiêu. Tìm kiếm được phép đi qua nghiệm vi phạm, rồi **trọng số phạt điều chỉnh động** kéo nghiệm về khả thi.

---

## 4. Hàm mục tiêu có phạt

```
F(S) = dist(S) + Σ_i α_i · V_i(S)
```

| Ràng buộc `i` | Vi phạm `V_i(S)` |
|---|---|
| Dung lượng (CVRP) | `V_cap = Σ_route max(0, load(route) − Q)` |
| Time window (VRPTW) | `V_tw = Σ_visit max(0, a_i − t_i, t_i − b_i)` (đến sớm/trễ) |

- `dist(S)`: tổng độ dài các cạnh.
- `α_i`: **trọng số phạt** (penalty parameter), điều chỉnh động.
- `V_i` nên được **chuẩn hóa** về cùng đơn vị độ dài để α có ý nghĩa (trong LKH-3 chính thức, các vi phạm được quy đổi về "đơn vị khoảng cách tương đương").

**Vì sao dùng phạt mà không dùng nhánh-cận ràng buộc cứng?** Vì nghiệm infeasible tạm thời là "cầu nối" giữa hai nghiệm feasible tốt mà giữa chúng không có đường đi khả thi thuần túy.

---

## 5. Điều chỉnh phạt động (mấu chốt thứ hai)

Sau mỗi trial, nhìn vào **incumbent** (nghiệm tốt nhất tìm được):

```
if incumbent feasible:      α_i ← max(α_min, α_i / decay)     # giảm phạt (ưu tiên rút ngắn đường)
else:                       α_i ← min(α_max, α_i · growth)    # tăng phạt (ưu tiên sửa vi phạm)
```

- `decay ≈ 1.5–2.0`, `growth ≈ 1.5–2.0`.
- Nếu incumbent vẫn infeasible → phạt tăng dần, buộc tìm kiếm "sửa" vi phạm.
- Khi incumbent feasible → phạt giảm dần, cho phép tìm kiếm mạo hiểm hơn để rút ngắn đường.
- Đây là một dạng **vòng lặp phản hồi tự cân bằng**: tìm kiếm dao động quanh biên khả thi, khai thác cả hai phía.

---

## 6. Candidate set (α-nearness)

Để move chỉ duyệt một phần nhỏ các cạnh, LKH tính cho mỗi nút `u` một tập `Candidate(u)` gồm `k` nút "gần/hứa hẹn" nhất (thường `k ≈ 5`).

Cách đo kinh điển là **α-nearness**:

```
α-nearness(u, v) = c(u, v) − π(u) − π(v)   , π = thế năng từ 1-tree
```

Một cạnh `(u,v)` được đưa vào candidate set nếu `α-nearness` nhỏ (tức cạnh vừa ngắn, vừa ít "cạnh tranh" với cây 1-tree). Ở dạng đơn giản, có thể thay bằng **k-nearest-neighbor** theo khoảng cách — cùng tinh thần, dễ cài hơn.

**Vai trò:** candidate set là lý do LKH nhanh. Bản code kèm theo *chưa* cài candidate set (duyệt lân cận đầy đủ) để giữ code dễ đọc; đây là điểm tăng tốc số một khi mở rộng lên `n` lớn.

---

## 7. Sequential k-opt move

Một move "tuần tự" là dãy xen kẽ các cạnh **bỏ đi (broken)** và **thêm vào (closed)**:

```
t1 → t2 → t3 → ... → t2k        # các cạnh (t1,t2),(t3,t4),... bị bỏ; (t2,t3),(t4,t5),... được thêm
```

Dây chuyền được mở rộng tham lam, **chỉ giữ khi gain dương** (theo `F`), và phải **đóng kín thành nghiệm hợp lệ** khi kết thúc. Một sequential move có thể "bao gồm" nhiều 2-opt, relocate, swap, Or-opt trong một bước.

Trong code tham khảo, để dễ đọc, sequential move được **thay bằng 4 họ move tường minh**:

| Move | Loại | Mô tả |
|---|---|---|
| 2-opt | nội tuyến | đảo đoạn khách `[i..j]` trong một tuyến |
| relocate | liên tuyến | chuyển 1 khách sang tuyến khác |
| swap | liên tuyến | đổi chỗ 2 khách giữa 2 tuyến |
| 2-opt\* | liên tuyến | nối chéo phần đuôi 2 tuyến |

Tất cả đều đánh giá theo `F(S, α)` → tự động cân nhắc cả khoảng cách lẫn vi phạm.

---

## 8. Cấu trúc tổng thể

```
1. Tính candidate set (tùy chọn).
2. Sinh nghiệm khởi tạo (greedy NN hoặc Clarke-Wright savings).
3. Lặp nhiều trial (multi-start):
   3.1. Điều chỉnh α theo tính khả thi của incumbent.
   3.2. Local search (sequential k-opt) với α hiện tại.
   3.3. Cập nhật incumbent + nghiệm khả thi tốt nhất.
   3.4. Nhiễu loạn (perturb) để thoát local optimum.
4. Trả về nghiệm khả thi tốt nhất (hoặc incumbent nếu chưa có nghiệm khả thi).
```

---

## 9. Độ phức tạp & hiệu năng

- Mỗi lần đánh giá một move trong code tham khảo là `O(n)` (dựng lại nghiệm). Với lân cận `O(n²)` và `R` trial → cỡ `O(R · n³)`. → Chỉ nên chạy với `n ≤ ~200`.
- LKH chính thức hạ xuống còn `O(n·k)` mỗi lần quét nhờ:
  - **candidate set** `k` (số cạnh ứng viên mỗi nút),
  - **đánh giá delta O(1)** cho từng move,
  - **đồ thị thu gọn** (segment tree / tour representation).

Kết quả: LKH-3 chính thức giải được CVRP/VRPTW hàng nghìn khách, thường đạt hoặc rất gần nghiệm tối ưu.

---

## 10. So sánh nhanh

| Phương pháp | Đặc điểm |
|---|---|
| Local search thuần (2-opt/Or-opt) | Đơn giản, nhanh nhưng kẹt local optimum, khó xử lý ràng buộc |
| Tabu Search / ALNS | Linh hoạt, cần tuning nhiều, mạnh trên VRPTW lớn |
| LKH-3 | Mạnh nhờ sequential k-opt + candidate set; ít tham số; là "chuẩn mực" cho CVRP/VRPTW |

---

## 11. Liên hệ với TTP (Traveling Thief Problem)

TTP = TSP + bài toán cái túi (knapsack) **liên kết với nhau**: tốc độ di chuyển phụ thuộc trọng lượng đồ đang mang. LKH-3 giải phần **định tuyến (VRP)**; với TTP bạn thường:

- dùng LKH(-3) để giải bài toán con **tour TSP**,
- xử lý phần chọn đồ (knapsack) bằng DP/local search riêng,
- ghép lại trong khung EA/ACO của bạn.

Cơ chế **penalty động** của LKH-3 cũng là một mẫu tốt để xử lý các ràng buộc mềm trong TTP.

---

## 12. Tài liệu tham khảo

- **Bài gốc LKH-3**: K. Helsgaun, *"An Extension of the Lin-Kernighan-Helsgaun TSP Solver for Constrained Traveling Salesman and Vehicle Routing Problems"* (2017).
- **Bài gốc LKH-2**: K. Helsgaun, *"An Effective Implementation of the Lin-Kernighan Traveling Salesman Heuristic"* (2000).
- **Code chính thức** (C, ~50k dòng, đầy đủ CVRP/VRPTW/MTSP): <http://akira.ruc.dk/~keld/research/LKH-3/>
- **Paper tham chiếu LKH**: S. Lin & B. W. Kernighan (1973), *"An Effective Heuristic Algorithm for the TSP"*.
