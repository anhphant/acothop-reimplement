# acothop — ACO for THOP

Ant Colony Optimization cho bài toán THOP (Team Orienteering Problem). Được tái cấu trúc từ ~16 bản copy lộn xộn về 1 repo duy nhất — xem `PLAN.md` để biết lịch sử & lý do.

## Cấu trúc

| Thư mục | Nội dung |
|---|---|
| `src/` | Code gốc (base = biến thể `original`) |
| `experiments/` | Các biến thể thuật toán chính — chỉ chứa file khác base + `variant.patch` |
| `experiments-legacy/` | Biến thể cũ (nhánh `1908fix-opt`) |
| `experiments-2phaseconnect/` | Thí nghiệm 2-phase connect (+ ablation) |
| `experiments-a280/` | Thí nghiệm setup a280 |
| `experiments-esaco/` | Thí nghiệm ESACO |
| `benchmark/`, `instances/` | Dữ liệu benchmark dùng chung |
| `results/` | Kết quả (CSV + solutions) |
| `scripts/` | Script phân tích / so sánh |
| `reference/` | Tài liệu tham khảo (LKH3, ESACO) |
| `docs/` | Ghi chú |

## Build & chạy

- **Base:** biên dịch `src/not-simple-2-tuneopt/main.cpp`
- **Biến thể:** `make exp-<tên>` = copy base → overlay file biến thể → build

## Các biến thể chính (`experiments/`)

- `change-score-top-K` — đổi điểm top-K
- `change-score-top-K-fibo`
- `multi-pheromone` — đa pheromone
- `multi-pheromone-notoptpack`
- `original-2phaseconnect-opt2`
- `original-fibo`
