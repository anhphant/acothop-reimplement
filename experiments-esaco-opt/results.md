# ESACO vs ACO++ — Thuc nghiem tren pla33810_10 (338.080 items)

- **Ngay:** 2026-11-06
- **Binary:** cung 1 chuong trinh, chung toan bo tham so. `--esaco 0` = origin (ACO++), `--esaco 1` = ESACO (S1: kNN + pheromone injection; S2: sparse pheromone trail; S3: tiered active list).
- **Setup:** 27 bo test `pla33810_10_{bsc,unc,usw}_{01,05,10}_{01..03}`, seed 10, time-limit 900s, 10 ants, alpha1=1, alpha2=2, rho=0.5, local search on, nn=20.
- **gap%** = (esaco − origin)/origin x 100. Duong = esaco tot hon.
- 5/54 run bi SSH cat ~1s truoc time-limit; gia tri lay lai tu best-so-far trong `.sol.log` (danh dau `*`).

## Ket qua chi tiet

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

## Tong hop

| Chi so | Gia tri |
|---|---:|
| W/T/L | 13 / 1 / 13 |
| Mean gap | **+0.22%** |
| Median gap | 0.00% |
| Best / Worst | +6.29% / -6.70% |
| Generation (mean) | esaco 29.4 vs origin 28.0 |

| nhom | n | mean | median | W/T/L |
|---|---:|---:|---:|---|
| bsc | 9 | +0.32% | +0.39% | 5/1/3 |
| unc | 9 | -0.15% | -0.07% | 4/0/5 |
| usw | 9 | +0.50% | -1.02% | 4/0/5 |
| cap 01 | 9 | +1.18% | -0.04% | 4/0/5 |
| cap 05 | 9 | -0.44% | -0.65% | 4/0/5 |
| cap 10 | 9 | -0.08% | +0.40% | 5/1/3 |

## Ket luan

1. Tren instance lon nhat (pla33810_10, 338.080 items): **ESACO nhinh hon origin mot chut** — mean +0.22%, W/T/L 13/1/13. Phan bo gap doi xung quanh 0 (median 0.00%, dao dong ±7%) nen voi 1 seed, ket luan trung thuc la **"ngang ngua, nghieng nhe ve ESACO"**.
2. **ESACO nhanh hon mot chut moi generation** (29.4 vs 28.0) — loi the tich luy khi budget tang.
3. Xu huong: ESACO cang co loi the khi instance cang lon — phu hop thiet ke (S2 sparse pheromone giam bo nho, S3 active list giam chi phi moi generation).
4. Can chay them nhieu seed de kiem dinh +0.22% co khac 0 khong.

## Files

- `src/esaco/` — ma nguon ESACO (cung binary chay ca origin va esaco)
- `scripts/run_esaco_origin_all.py` — runner: compile + chay 27 x 2 task, 900s/run
- `scripts/analyze.py` — tinh bang gap + thong ke tu `results/summary.csv`
- `results/summary.csv` — ket qua thuc nghiem (54 dong)
- `results/gap.csv` — bang so sanh cuoi cung (27 cap)
