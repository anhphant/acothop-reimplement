# LKH-3 — Pseudocode chi tiết

> Bản mã giả khớp 1-1 với file `lkh3.py`. Mục tiêu CVRP (dung lượng); VRPTW chỉ thêm một số hạng phạt tương tự.

---

## 0. Ký hiệu

- `D[i][j]` : khoảng cách giữa nút `i` và `j`.
- `demand[v]`, `capacity Q`.
- Nghiệm `S = {r_1, r_2, ...}`, mỗi tuyến `r = [0, v1, ..., vk, 0]`.
- `load(r) = Σ demand[v], v ∈ r`.
- `dist(r) = Σ D[r[i]][r[i+1]]`.
- `V_cap(S) = Σ_r max(0, load(r) − Q)`.
- `F(S, α) = total_dist(S) + α · V_cap(S)`.

---

## 1. Vòng lặp chính

```
SOLVE(instance, alpha0, growth, decay, alpha_min, alpha_max, restarts):

    S       ← CLARKE_WRIGHT_INIT(instance)      # hoặc GREEDY_INIT
    α       ← alpha0
    S_best  ← S
    F_best  ← F(S, α)
    S_feasible ← None          # nghiệm KHẢ THI tốt nhất (đáp án thực tế)
    stall   ← 0

    for it = 1 .. restarts:

        # ---- (1.1) local search với α hiện tại ----
        loop:
            S' ← IMPROVE(S, α)                  # best-improvement
            if S' = None: break
            S ← S'
            if F(S, α) < F_best:
                S_best, F_best ← S, F(S, α)
                stall ← 0
            else:
                stall ← stall + 1
            if V_cap(S) ≤ 0 and total_dist(S) < best_feasible_dist:
                S_feasible ← S

        # ---- (1.2) điều chỉnh phạt theo incumbent ----
        if V_cap(S_best) ≤ 0:
            α ← max(alpha_min, α · decay)       # incumbent feasible → giảm phạt
        else:
            α ← min(alpha_max, α · growth)      # incumbent infeasible → tăng phạt

        if stall > 50: break
        S ← PERTURB(S_best)                     # nhiễu loạn, thoát local optimum

    return S_feasible if S_feasible ≠ None else S_best
```

---

## 2. Khởi tạo

### 2.1 Greedy nearest-neighbor

```
GREEDY_INIT(instance):
    unserved ← {1..n} (xáo trộn ngẫu nhiên)
    S ← []
    while unserved ≠ ∅:
        r ← [0]; load ← 0; cur ← 0
        loop:
            v* ← argmin_{v ∈ unserved, load + demand[v] ≤ Q} D[cur][v]
            if không có v*: break
            r.append(v*); unserved.remove(v*)
            load ← load + demand[v*]; cur ← v*
        r.append(0); S.append(r)
    return S
```

### 2.2 Clarke-Wright savings (biến thể tuần tự / Gaskell)

```
CLARKE_WRIGHT_INIT(instance):
    unserved ← {1..n}
    S ← []
    while unserved ≠ ∅:
        u ← argmax_{v ∈ unserved} D[0][v]        # mở tuyến từ khách xa depot nhất
        r ← [0, u, 0]; load ← demand[u]; unserved.remove(u)
        loop:
            best_s ← −1; best_v ← None; best_side ← None
            for v ∈ unserved with load + demand[v] ≤ Q:
                e_left  ← r[1]                       # đầu trái
                e_right ← r[-2]                      # đầu phải
                s_left  ← D[0][e_left]  + D[0][v] − D[e_left][v]
                s_right ← D[0][e_right] + D[0][v] − D[e_right][v]
                giữ (s, v, side) lớn nhất
            if best_v = None: break
            if best_side = left : r ← [0, best_v] + r[1:]
            else                 : r ← r[:-1] + [best_v, 0]
            load ← load + demand[best_v]; unserved.remove(best_v)
        S.append(r)
    return S
```

---

## 3. Local search (best improvement)

```
IMPROVE(S, α):
    F_cur   ← F(S, α)
    best_delta ← 0
    best_S  ← None
    for S' ∈ NEIGHBORS(S):
        δ ← F(S', α) − F_cur
        if δ < best_delta:
            best_delta ← δ; best_S ← S'
    return best_S            # None nếu không còn move cải thiện
```

---

## 4. Các họ move (NEIGHBORS)

### 4.1 2-opt nội tuyến (đảo đoạn)

```
for mỗi tuyến r, m = len(r):
    for i = 1 .. m−3:
        for j = i+1 .. m−2:
            r' ← r[:i] + reverse(r[i..j]) + r[j+1:]
            yield S với tuyến r thay bằng r'
```

### 4.2 Relocate liên tuyến (chuyển 1 khách)

```
for tuyến a, vị trí i (khách v = r_a[i]):
    for tuyến b ≠ a, vị trí chèn j:
        r_a' ← r_a bỏ v
        r_b' ← chèn v vào giữa r_b[j−1] và r_b[j]
        yield S mới
```

### 4.3 Swap liên tuyến (đổi 2 khách)

```
for tuyến a, khách u = r_a[i]:
    for tuyến b > a, khách v = r_b[j]:
        r_a' ← r_a với u thay bằng v
        r_b' ← r_b với v thay bằng u
        yield S mới
```

### 4.4 2-opt\* liên tuyến (nối chéo đuôi)

```
for tuyến a, vị trí i (cắt sau khách r_a[i]):
    for tuyến b > a, vị trí j (cắt sau khách r_b[j]):
        r_a' ← r_a[0..i] + r_b[j+1..end]
        r_b' ← r_b[0..j] + r_a[i+1..end]
        yield S mới
```

> **Ghi chú:** đây là 4 move tường minh thay thế cho "sequential k-opt tổng quát" của LKH thật. Sequential move có thể coi là một chuỗi ghép các loại trao đổi trên thành một move duy nhất với gain dương.

---

## 5. Nhiễu loạn (perturb)

```
PERTURB(S, moves=5):
    S' ← các tuyến không rỗng của S
    lặp moves lần:
        chọn tuyến a (có khách), rút ngẫu nhiên 1 khách v
        chèn v vào vị trí ngẫu nhiên của một tuyến b
    return S'
```

*(LKH thật dùng double-bridge 4-opt + khởi tạo ngẫu nhiên mỗi trial.)*

---

## 6. Điều chỉnh phạt

```
ADJUST_PENALTY(α, incumbent_feasible, growth, decay, α_min, α_max):
    if incumbent_feasible:
        return max(α_min, α · decay)     # giảm phạt
    else:
        return min(α_max, α · growth)    # tăng phạt
```

---

## 7. Mở rộng VRPTW (tóm tắt)

- Thêm `V_tw(S) = Σ_visit max(0, a_i − t_i, t_i − b_i)`.
- Hàm mục tiêu: `F(S) = total_dist(S) + α_cap · V_cap(S) + α_tw · V_tw(S)`.
- Khi đánh giá move phải **tính lại** thời gian đến `t_i` (O(độ dài tuyến)) để ra `V_tw`.
- Điều chỉnh cả `α_cap` lẫn `α_tw` bằng cùng quy tắc ở mục 6.
