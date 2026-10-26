# Tổng hợp các cải tiến của `current-best-algo`

Dưới đây là tóm tắt toàn bộ các kỹ thuật tối ưu hóa và tinh chỉnh thuật toán đã được tích hợp vào `current-best-algo` để giúp nó bứt phá từ mốc lợi nhuận cơ bản (4.6 Triệu) lên mức **4.72 Triệu**, đồng thời giảm thời gian chạy xuống mức thấp nhất có thể.

## 1. Ultra-fast TSP Optimization (UTO) - Tối ưu hóa phễu lọc
* **Vấn đề của thuật toán gốc:** Khâu nhặt đồ (Knapsack Packing) rất tốn kém thời gian vì phải thử nghiệm `--ptries` lần (thường là 10 lần) cho **toàn bộ 40 con kiến** trong mỗi thế hệ.
* **Giải pháp UTO:** Trước khi tiến hành Packing, `current-best-algo` sẽ duyệt qua một vòng Local Search cực nhanh cho tất cả các kiến, sau đó **xếp hạng** chúng dựa trên độ dài đường đi thuần túy (pure geometric TSP distance). 
* Thuật toán chỉ chọn ra **Top 5 con kiến** có đường đi TSP ngắn nhất để chạy hàm Packing đắt đỏ. Các con kiến còn lại sẽ bị bỏ qua do đường đi quá dài, không có tiềm năng sinh ra lợi nhuận cao mà không vi phạm `maxTime`. Điều này giúp tiết kiệm 80-90% thời gian vô ích.

## 2. Hàm mục tiêu đa tham số (Multi-parametric Fitness)
* **Vấn đề của thuật toán gốc:** Trong ACO truyền thống cho THOP/TTP, kiến chỉ ngửi mùi Pheromone và Heuristic dựa trên khoảng cách địa lý (Heuristic = `1 / distance`). Chúng hoàn toàn "mù" về trọng lượng và thời gian cho tới khi thuật toán Packing chạy ở bước cuối.
* **Giải pháp:** Bổ sung các trọng số $\alpha_1, \alpha_2, \alpha_3, \alpha_4$ vào hàm đánh giá (Fitness) của kiến trong quá trình di chuyển. Kiến không chỉ bị hấp dẫn bởi khoảng cách ngắn, mà còn bị chi phối bởi các yếu tố:
    * Khả năng chứa của xe (Capacity).
    * Giới hạn thời gian sinh tồn (Max Time).
    * Mức lợi nhuận kỳ vọng của cụm thành phố phía trước.
* Việc này giúp kiến tự động tìm ra các con đường bao quanh các "cụm đồ vật giá trị cao" thay vì chỉ vẽ ra một đường đi TSP ngắn một cách vô thức.

## 3. Thuật toán "Nhặt đồ muộn" (Late Harvest Optimization)
* **Vấn đề của thuật toán gốc:** Thuật toán Greedy mặc định tính điểm bằng công thức $\frac{Profit}{Weight}$, khiến nó có xu hướng nhặt những món đồ nhẹ-giá-trị-cao ngay từ **những thành phố đầu tiên**. Hậu quả là xe bị nặng từ rất sớm, vận tốc giảm sút trong toàn bộ quãng đường còn lại (làm cạn kiệt maxTime rất nhanh).
* **Giải pháp:** Cải tiến lại hàm tính Score của Packing để bổ sung tham số Penalty. Các món đồ nằm ở nửa đầu chuyến đi sẽ bị "phạt" điểm, trong khi những món nằm sát điểm đích (End City) sẽ được "thưởng" điểm cộng lớn. 
* Nhờ cơ chế này, `current-best-algo` sẽ cố gắng chạy xe "rỗng" ở giai đoạn đầu để đi thật xa và thật nhanh, sau đó "quét sạch" đồ vật trên đường quay trở về đích.

## 4. Distance Matrix O(1)
* **Vấn đề của thuật toán gốc:** Việc phải gọi hàm `sqrt()` và `ceil()` lặp đi lặp lại hàng triệu lần trong quá trình kiến di chuyển và Local Search làm tiêu tốn lượng lớn tài nguyên CPU.
* **Giải pháp:** Khởi tạo một mảng 2 chiều (Ma trận khoảng cách) `dist_matrix[i][j]` ngay từ đầu bằng Memoization. Việc lấy khoảng cách giữa 2 thành phố giờ đây chỉ là tra cứu mảng với thời gian $O(1)$. Tăng tốc độ thuật toán tổng thể lên nhiều lần.

## 5. Candidate List (k-Nearest-Neighbors) cho Local Search
* **Vấn đề của thuật toán gốc:** Thuật toán 2-opt, 2.5-opt và 3-opt mặc định sẽ quét qua mọi cặp đỉnh $i, j$ trong toàn bộ 1000 đỉnh, tạo ra độ phức tạp $O(N^3)$.
* **Giải pháp:** Tại bước khởi tạo (`ACOinit`), một danh sách láng giềng `knn` (với K=20) được thiết lập cho từng thành phố. Khi Local Search thực hiện gỡ chéo (2-opt) hay dời đỉnh (0.5-opt), nó chỉ tìm kiếm và nối lại các đỉnh nằm trong tập 20 láng giềng gần nhất này. Độ phức tạp giảm từ $O(N^3)$ xuống $O(N \times K)$, giúp Local Search có thể quét 20 vòng (max_passes=20) trong tích tắc.

---

**Tương lai:** Để chinh phục mốc **4.8 Triệu**, `current-best-algo` sẽ cần loại bỏ phương pháp nhặt đồ ngẫu nhiên (Greedy Heuristic) và thay thế bằng thuật toán **Dynamic Programming (Quy hoạch động)** kết hợp Knapsack Local Search để vét sạch những kilogram cuối cùng của xe chở hàng.
