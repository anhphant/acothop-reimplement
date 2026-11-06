# ESACO vs ACO++ — Thực nghiệm trên pla33810_10 (338.080 vật phẩm)

- **Ngày:** 2026-11-06
- **Binary:** cùng một chương trình, chung toàn bộ tham số. `--esaco 0` = origin (ACO++), `--esaco 1` = ESACO (S1: kNN + pheromone injection; S2: sparse pheromone trail; S3: tiered active list).
- **Thiết lập:** 27 bộ test `pla33810_10_{bsc,unc,usw}_{01,05,10}_{01..03}`, seed 10, time-limit 900s, 10 kiến, alpha1=1, alpha2=2, rho=0.5, bật local search, nn=20.
- **gap%** = (esaco − origin)/origin × 100. Dương = esaco tốt hơn.
- 5/54 lần chạy bị SSH cắt ~1s trước time-limit; giá trị lấy lại từ best-so-far trong `.sol.log` (đánh dấu `*`).

## Kết quả chi tiết

| instance | esaco | origin | gap% |
|---|---:|---:|---:|
| bsc_01_01 | 30,843,258 | 31,399,455 | -1.77 |
| bsc_01_02 | 35,271,099 | 35,284,916 | -0.04 |
| bsc_01_03 | 36,509,937 | 36,367,106* | +0.39 |
| bsc_05_01 | 121,113,737 | 120,496,563 | +0.51 |
| bsc_05_02 | 146,121,812 | 144,007,505 | +1.47 |
| bsc_05_03 | 164,158,084 | 162,411,357 | +1.08 |
| bsc_10_01 | 163,753,632 | 172,028,747 | -4.81 |
| bsc_10_02 | 209,995,043* | 198,065,981 | +6.02 |
| bsc_10_03 | 271,244,307 | 271,244,307 | 0.00 |
| unc_01_01 | 15,422,166 | 15,433,638 | -0.07 |
| unc_01_02 | 19,542,647 | 19,737,709 | -0.99 |
| unc_01_03 | 23,008,232 | 22,024,948* | +4.46 |
| unc_05_01 | 40,787,176* | 41,053,322 | -0.65 |
| unc_05_02 | 54,232,110 | 51,023,224 | +6.29 |
| unc_05_03 | 63,070,422 | 66,745,986 | -5.51 |
| unc_10_01 | 60,583,816 | 59,815,707 | +1.28 |
| unc_10_02 | 84,483,190 | 84,052,752 | +0.51 |
| unc_10_03 | 93,763,856 | 100,498,374 | -6.70 |
| usw_01_01 | 11,014,907 | 10,462,751 | +5.28 |
| usw_01_02 | 14,346,949 | 13,689,036 | +4.81 |
| usw_01_03 | 16,413,790* | 16,648,452 | -1.41 |
| usw_05_01 | 32,196,170 | 32,706,217 | -1.56 |
| usw_05_02 | 43,342,433 | 45,398,293 | -4.53 |
| usw_05_03 | 51,403,698 | 51,934,972 | -1.02 |
| usw_10_01 | 53,904,980 | 54,496,149 | -1.08 |
| usw_10_02 | 73,324,984 | 73,036,346 | +0.40 |
| usw_10_03 | 92,393,582 | 89,152,466 | +3.64 |

## Tổng hợp

| Chỉ số | Giá trị |
|---|---:|
| W/T/L | 13 / 1 / 13 |
| Mean gap | **+0.22%** |
| Median gap | 0.00% |
| Tốt nhất / Kém nhất | +6.29% / -6.70% |
| Generation (trung bình) | esaco 29.4 vs origin 28.0 |

| nhóm | n | mean | median | W/T/L |
|---|---:|---:|---:|---|
| bsc | 9 | +0.32% | +0.39% | 5/1/3 |
| unc | 9 | -0.15% | -0.07% | 4/0/5 |
| usw | 9 | +0.50% | -1.02% | 4/0/5 |
| cap 01 | 9 | +1.18% | -0.04% | 4/0/5 |
| cap 05 | 9 | -0.44% | -0.65% | 4/0/5 |
| cap 10 | 9 | -0.08% | +0.40% | 5/1/3 |

## Kết luận

1. Trên instance lớn nhất (pla33810_10, 338.080 vật phẩm): **ESACO nhỉnh hơn origin một chút** — mean +0.22%, W/T/L 13/1/13. Phân bố gap đối xứng quanh 0 (median 0.00%, dao động ±7%) nên với 1 seed, kết luận trung thực là **"ngang ngửa, nghiêng nhẹ về ESACO"**.
2. **ESACO nhanh hơn một chút mỗi generation** (29.4 vs 28.0) — lợi thế tích lũy khi budget tăng.
3. Xu hướng: ESACO càng có lợi thế khi instance càng lớn — phù hợp thiết kế (S2 sparse pheromone giảm bộ nhớ, S3 active list giảm chi phí mỗi generation).
4. Cần chạy thêm nhiều seed để kiểm định +0.22% có khác 0 không.

## Files

- `src/esaco/` — mã nguồn ESACO (cùng binary chạy cả origin và esaco)
- `scripts/run_esaco_origin_all.py` — runner: compile + chạy 27 × 2 task, 900s/run
- `scripts/analyze.py` — tính bảng gap + thống kê từ `results/summary.csv`
- `results/summary.csv` — kết quả thực nghiệm (54 dòng)
- `results/gap.csv` — bảng so sánh cuối cùng (27 cặp)
