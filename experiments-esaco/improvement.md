# Các cải tiến của ESACO (từ bài báo gốc)

Nguồn: *Effective heuristics for ant colony optimization to handle large-scale problems* — H. Ismkhan, Swarm and Evolutionary Computation 32 (2017). ESACO = Effective Strategies + ACO.

## 1. Dùng pheromone trong Local Search (candidate-set động)

- Khởi tạo candidate-set bằng nearest-neighbors.
- Sau mỗi bước, với mỗi nút `n`:
  - Chèn nút có pheromone cực đại vào đầu candidate-set.
  - Chèn nút kế tiếp của `n` trong best tour vào đầu candidate-set.
  - Xóa nút cuối (cấu trúc dạng hàng đợi, không trùng lặp).
- Nhờ vậy Local Search (2-opt) dùng candidate-set phản ánh thông tin pheromone thay vì tập cố định.

## 2. Biểu diễn pheromone thưa (sparse)

- Không lưu ma trận đầy đủ O(n²); chỉ lưu các cung `(i,j)` có giá trị pheromone khác giá trị khởi tạo `τ₀`.
- Giới hạn kích thước danh sách mỗi nút bằng `Max-Size` (= 10): nếu vượt quá, cắt phần tử có pheromone thấp nhất.
- Độ phức tạp không gian giảm từ O(n²) xuống O(n) (thực tế < 50·n trên các instance lớn).

## 3. Tăng tốc chọn bước đi (transition rule)

Thứ tự thăm dò để chọn nút kế tiếp của nút `r`:
1. Thăm dò candidate-set của `r` trước.
2. Nếu không chọn được, thăm dò các nút có pheromone đã cập nhật (≠ `τ₀`) trong danh sách thưa.
3. Cuối cùng mới quét toàn bộ các nút chưa thăm.

Hit-rate > 0.97: 97% số lần chọn bước đi chỉ cần thăm dò 2 tập nhỏ, không quét toàn bộ instance.

## Tham số bài báo

- Nearest neighbors: 4
- Max-Size: 10
- α (pheromone weight) = 0.9
- β (heuristic weight) = 2.0
- ρ (evaporation) = 0.6
