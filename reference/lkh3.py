"""
LKH-3 — teaching reference cho CVRP (Capacitated Vehicle Routing Problem).

Tái hiện các ý tưởng cốt lõi của LKH-3 (Keld Helsgaun, 2017):
  1. Hàm mục tiêu có phạt (penalty):  F(S) = dist(S) + alpha * capacity_violation(S)
  2. Cho phép duyệt nghiệm infeasible trong khi tìm kiếm; phạt động kéo về feasible
  3. Điều chỉnh alpha động theo tính khả thi của incumbent
  4. Local search kiểu LKH: 2-opt (nội tuyến), relocate, swap, 2-opt* (liên tuyến)
  5. Khởi tạo: greedy nearest-neighbor hoặc Clarke-Wright savings

Giả định: demand[i] <= capacity với mọi khách i (instance khả thi).

Đây KHÔNG phải bản C chính thức (~50k dòng, http://akira.ruc.dk/~keld/research/LKH-3/);
đây là bản compact, dễ đọc để học/hiểu cơ chế. Các khác biệt chính:
  - Thay "sequential k-opt tổng quát" bằng 4 họ move tường minh.
  - Chưa cài candidate set (α-nearness) -> duyệt lân cận đầy đủ (chỉ nên dùng n <= ~200).
  - Đánh giá move bằng cách dựng lại nghiệm (O(n)) thay vì delta O(1).

Sử dụng:
  python lkh3.py
"""

import math
import random


class CVRP:
    def __init__(self, coords, demand, capacity):
        """
        coords  : list[(x, y)] - tọa độ nút; coords[0] = depot.
        demand  : list[float]  - nhu cầu; demand[0] = 0.
        capacity: float        - tải trọng xe Q.
        """
        self.coords = coords
        self.demand = demand
        self.capacity = capacity
        self.n = len(coords)
        # ma trận khoảng cách Euclid
        self.D = [[0.0] * self.n for _ in range(self.n)]
        for i in range(self.n):
            for j in range(self.n):
                dx = coords[i][0] - coords[j][0]
                dy = coords[i][1] - coords[j][1]
                self.D[i][j] = math.hypot(dx, dy)

    # ----------------------------------------------------------------
    # Đánh giá nghiệm (một nghiệm = list các tuyến; mỗi tuyến = [0, ..., 0])
    # ----------------------------------------------------------------
    def route_load(self, r):
        """Tổng nhu cầu của một tuyến."""
        return sum(self.demand[v] for v in r)

    def route_dist(self, r):
        """Độ dài một tuyến."""
        return sum(self.D[r[i]][r[i + 1]] for i in range(len(r) - 1))

    def total_dist(self, sol):
        return sum(self.route_dist(r) for r in sol)

    def capacity_violation(self, sol):
        """Tổng phần tải vượt dung lượng trên mọi tuyến (>= 0)."""
        return sum(max(0.0, self.route_load(r) - self.capacity) for r in sol)

    def objective(self, sol, alpha):
        """Hàm mục tiêu có phạt của LKH-3."""
        return self.total_dist(sol) + alpha * self.capacity_violation(sol)

    def is_feasible(self, sol):
        return self.capacity_violation(sol) <= 1e-9

    # ----------------------------------------------------------------
    # Khởi tạo
    # ----------------------------------------------------------------
    def greedy_init(self, seed=None):
        """Greedy nearest-neighbor: mỗi tuyến đi khách gần nhất còn tải được."""
        rng = random.Random(seed)
        unserved = list(range(1, self.n))
        rng.shuffle(unserved)
        sol = []
        while unserved:
            route, load, cur = [0], 0.0, 0
            while True:
                best, bestd = None, float("inf")
                for v in unserved:
                    if load + self.demand[v] <= self.capacity + 1e-9:
                        d = self.D[cur][v]
                        if d < bestd:
                            bestd, best = d, v
                if best is None:
                    break
                route.append(best)
                unserved.remove(best)
                load += self.demand[best]
                cur = best
            route.append(0)
            sol.append(route)
        return sol

    def clarke_wright_init(self, seed=None):
        """
        Clarke-Wright savings (biến thể tuần tự / Gaskell) có ràng buộc tải.
        Mỗi bước gắn khách chưa định tuyến vào một đầu của tuyến hiện tại
        theo savings: s(v, e) = d(0,v) + d(0,e) - d(v,e), chọn s lớn nhất.
        (Xác định - không dùng seed.)
        """
        D, Q = self.D, self.capacity
        unrouted = set(range(1, self.n))
        sol = []
        while unrouted:
            # mở tuyến mới từ khách xa depot nhất (xử lý "vành ngoài" trước)
            u = max(unrouted, key=lambda v: D[0][v])
            route = [0, u, 0]
            load = self.demand[u]
            unrouted.remove(u)
            while True:
                best_s, best_v, best_side = -1.0, None, None
                for v in unrouted:
                    if load + self.demand[v] > Q + 1e-9:
                        continue
                    for side, e in ((0, route[1]), (1, route[-2])):
                        s = D[0][e] + D[0][v] - D[e][v]
                        if s > best_s:
                            best_s, best_v, best_side = s, v, side
                if best_v is None:
                    break
                if best_side == 0:                      # gắn vào đầu trái
                    route = [0, best_v] + route[1:]
                else:                                    # gắn vào đầu phải
                    route = route[:-1] + [best_v, 0]
                load += self.demand[best_v]
                unrouted.remove(best_v)
            sol.append(route)
        return sol

    # ----------------------------------------------------------------
    # Lân cận (neighborhood) - sinh các nghiệm ứng viên
    # ----------------------------------------------------------------
    def _neighbors(self, sol):
        """
        Sinh toàn bộ nghiệm lân cận. Bốn họ move tương ứng với các move mà
        "sequential k-opt" của LKH tổng quát hóa.
        """
        # (a) 2-opt nội tuyến: đảo đoạn khách [i..j] trong một tuyến.
        for a in range(len(sol)):
            r = sol[a]
            m = len(r)
            for i in range(1, m - 2):
                for j in range(i + 1, m - 1):
                    new = [list(x) for x in sol]
                    new[a] = r[:i] + r[i:j + 1][::-1] + r[j + 1:]
                    yield new

        # (b) relocate liên tuyến: chuyển khách v từ tuyến a sang vị trí j của tuyến b.
        for a in range(len(sol)):
            r = sol[a]
            for i in range(1, len(r) - 1):
                v = r[i]
                for b in range(len(sol)):
                    if b == a:
                        continue
                    t = sol[b]
                    # j = vị trí chèn giữa t[j-1] và t[j]
                    for j in range(1, len(t)):
                        new = [list(x) for x in sol]
                        new[a] = r[:i] + r[i + 1:]
                        new[b] = t[:j] + [v] + t[j:]
                        yield new

        # (c) swap liên tuyến: đổi khách u (tuyến a) với khách v (tuyến b).
        for a in range(len(sol)):
            r = sol[a]
            for i in range(1, len(r) - 1):
                u = r[i]
                for b in range(a + 1, len(sol)):
                    t = sol[b]
                    for j in range(1, len(t) - 1):
                        v = t[j]
                        new = [list(x) for x in sol]
                        new[a] = r[:i] + [v] + r[i + 1:]
                        new[b] = t[:j] + [u] + t[j + 1:]
                        yield new

        # (d) 2-opt* liên tuyến: nối chéo phần đuôi của hai tuyến.
        for a in range(len(sol)):
            r = sol[a]
            for i in range(1, len(r) - 1):
                for b in range(a + 1, len(sol)):
                    t = sol[b]
                    for j in range(1, len(t) - 1):
                        new = [list(x) for x in sol]
                        new[a] = r[:i + 1] + t[j + 1:]
                        new[b] = t[:j + 1] + r[i + 1:]
                        yield new

    # ----------------------------------------------------------------
    # Local search
    # ----------------------------------------------------------------
    def improve(self, sol, alpha):
        """
        Best improvement theo F(·, alpha): quét toàn bộ lân cận, áp dụng move
        giảm F nhiều nhất. Trả về nghiệm mới hoặc None nếu hết move cải thiện.
        """
        cur = self.objective(sol, alpha)
        best_delta, best_sol = 0.0, None
        for cand in self._neighbors(sol):
            d = self.objective(cand, alpha) - cur
            if d < best_delta - 1e-9:
                best_delta, best_sol = d, cand
        return best_sol

    def perturb(self, sol, moves=5, seed=None):
        """
        Nhiễu loạn để thoát local optimum: chuyển ngẫu nhiên vài khách.
        (LKH thật dùng double-bridge 4-opt + khởi tạo ngẫu nhiên mỗi trial.)
        """
        rng = random.Random(seed)
        s = [list(r) for r in sol if len(r) > 2]          # bỏ tuyến rỗng [0,0]
        for _ in range(moves):
            nonempty = [a for a in range(len(s)) if len(s[a]) > 2]
            if not nonempty:
                break
            a = rng.choice(nonempty)
            r = s[a]
            i = rng.randrange(1, len(r) - 1)
            v = r.pop(i)
            t = s[rng.randrange(len(s))]
            t.insert(rng.randrange(1, len(t)), v)
        return s

    # ----------------------------------------------------------------
    # Vòng lặp chính LKH-3
    # ----------------------------------------------------------------
    def solve(self, alpha0=5.0, growth=1.5, decay=0.5,
              alpha_min=1.0, alpha_max=1e5, restarts=200, seed=None,
              init="cw"):
        """
        Multi-start + điều chỉnh phạt động.

        init    : "cw" (Clarke-Wright) hoặc "greedy".
        alpha0  : phạt khởi tạo (phụ thuộc thang đo: nên cỡ (khoảng cách / đơn vị tải)).
        growth  : hệ số TĂNG alpha khi incumbent infeasible.
        decay   : hệ số GIẢM alpha khi incumbent feasible.
        restarts: số trial (multi-start).
        """
        sol = self.clarke_wright_init(seed) if init == "cw" else self.greedy_init(seed)
        alpha = alpha0
        best_sol, best_obj = sol, self.objective(sol, alpha)
        best_feasible, best_feasible_dist = None, float("inf")
        if self.is_feasible(sol):                       # ghi nhận nghiệm khởi tạo (luôn khả thi)
            best_feasible, best_feasible_dist = sol, self.total_dist(sol)
        stall = 0

        for it in range(restarts):
            # --- local search với alpha hiện tại ---
            while True:
                new = self.improve(sol, alpha)
                if new is None:
                    break
                sol = new
                o = self.objective(sol, alpha)
                if o < best_obj - 1e-9:
                    best_obj, best_sol, stall = o, sol, 0
                else:
                    stall += 1
                # ghi nhớ nghiệm khả thi tốt nhất (đáp án thực tế của CVRP)
                if self.is_feasible(sol):
                    d = self.total_dist(sol)
                    if d < best_feasible_dist - 1e-9:
                        best_feasible_dist, best_feasible = d, sol

            # --- điều chỉnh phạt theo tính khả thi của incumbent ---
            if self.is_feasible(best_sol):
                alpha = max(alpha_min, alpha * decay)     # giảm phạt
            else:
                alpha = min(alpha_max, alpha * growth)    # tăng phạt

            if stall > 50:
                break
            sol = self.perturb(best_sol, seed=it)

        return best_feasible if best_feasible is not None else best_sol

    # ----------------------------------------------------------------
    # Báo cáo
    # ----------------------------------------------------------------
    def report(self, name, sol):
        d = self.total_dist(sol)
        v = self.capacity_violation(sol)
        k = sum(1 for r in sol if len(r) > 2)          # số tuyến không rỗng
        loads = ", ".join(f"{self.route_load(r):.0f}" for r in sol if len(r) > 2)
        print(f"{name:9s} dist={d:10.2f}  viol={v:7.1f}  routes={k:2d}  "
              f"loads=[{loads}]")


# ----------------------------------------------------------------------
# Demo
# ----------------------------------------------------------------------
if __name__ == "__main__":
    random.seed(42)
    n_cust, capacity = 30, 40
    coords = [(50.0, 50.0)]                                # depot ở tâm
    demand = [0]
    for _ in range(n_cust):
        ang = random.uniform(0, 2 * math.pi)
        rad = random.uniform(5, 45)
        coords.append((50 + rad * math.cos(ang), 50 + rad * math.sin(ang)))
        demand.append(random.randint(1, 9))

    prob = CVRP(coords, demand, capacity)

    g = prob.greedy_init(seed=0)
    cw = prob.clarke_wright_init()
    sol = prob.solve(restarts=200, seed=1, init="cw")

    prob.report("greedy", g)
    prob.report("C-W", cw)
    prob.report("LKH3", sol)
