# Plan tái cấu trúc `acothop`

- Ngày lập: 2026-10-26
- Trạng thái: **Đã thực thi gần xong** (B0–B5 + B7-kiểm chứng xong; còn B6 xóa thư mục cũ + commit — bị chặn tạm thời bởi classifier)
- Phạm vi: toàn bộ `D:\PhD\THOP-TTP\acothop`

## 1. Mục tiêu

Gộp 1 project (`github.com/anhphant/acothop-reimplement` — ACO cho THOP) đang bị nhân bản thành ~16 thư mục về **1 repo duy nhất**, giữ nguyên toàn bộ các biến thể thuật toán, xóa rác và trùng lặp. Ước tính giảm từ ~25GB+ xuống ~1.5GB.

## 2. Chẩn đoán hiện trạng

**Bản chất:** tất cả thư mục đều là bản copy cùng remote, cùng commit gốc `ba0e59e`, chỉ khác nhau ở vài file source. Source thật nằm ở `src/not-simple-2-tuneopt/` (aco.cpp, packing.cpp, pheromone.cpp, problem/, colony/, cli/, ls/…).

**Các biến thể thuật toán thật sự (đối chiếu `aco.cpp` bằng md5) — chỉ 7 phiên bản khác nhau:**

| Nhóm | Thư mục giống hệt nhau | Giữ bản |
|---|---|---|
| A | `original` = `original-2phaseconnect` | `original` (base) |
| B | `change-score-top-K` = `change-score-top-K-stratified` | `change-score-top-K` |
| C | `change-score-top-K-fibo` | giữ |
| D | `multi-pheromone` | giữ |
| E | `multi-pheromone-notoptpack` | giữ |
| F | `original-2phaseconnect-opt2` = `original-2phaseconnect - Copy` | `original-2phaseconnect-opt2` |
| G | `original-fibo` = `original-fibofull` | `original-fibo` |

→ 9/16 thư mục là bản trùng lặp (được đại diện bởi 7 bản gốc).

**Rác & trùng lặp (đã đo):**

| Nguồn | Mỗi thư mục | × số bản | Tổng |
|---|---|---|---|
| `instances/` (bộ `.thop`) | 701MB | 16 | ~11GB trùng → giữ 1 bản |
| `.claude/worktrees/` (snapshot ẩn nhánh `1908fix-opt`) | 324MB | 14 | ~4.5GB → trích `experiments-legacy/` |
| `src/` (chứa `main.exe` 18MB + log) | 157MB | 16 | ~2.5GB → dọn |
| `benchmark/` (có `.git` lồng nhau) | 87MB | 16 | ~1.4GB → dọn |
| `exp-2phaseconnect-ssh.zip` (2.2GB) + `original-esaco.zip` (909MB) | — | — | 3.1GB → chuyển ổ ngoài |
| `ESACO-experiment/` | 4GB | 1 | riêng lẻ |

**Các thư mục không thuộc 7 biến thể trên (xử lý riêng):**
- `exp-2phaseconnect` / `exp-2phaseconnect-ssh`: harness thí nghiệm 2-phase (chứa `src-2p`, `src-origin`, script compare, results). `-ssh` nghi là fork của `exp-2phaseconnect` → diff khi thực thi.
- `experiment_setup_a280`: harness thí nghiệm (`src-multiphe`, `src-origin`, `src-topK`).
- `EA`: `lkh3.py` (LKH3 reference, 36KB) → giữ làm reference.
- `ESACO-experiment`: dòng thí nghiệm riêng (4GB).

## 3. Quyết định đã chốt

1. **Mô hình git:** 1 repo duy nhất; biến thể = thư mục `experiments/` (chỉ chứa file khác base, không copy toàn bộ).
2. **2 file zip lớn:** chuyển ra ổ ngoài, **không commit**.
3. **Dòng thí nghiệm ẩn `.claude/worktrees/` (nhánh `1908fix-opt`):** trích ra `experiments-legacy/` riêng.

## 4. Cấu trúc mục tiêu

```
acothop/
├── .git/
├── src/not-simple-2-tuneopt/        # source chuẩn (base = original)
├── experiments/                     # 6 biến thể root (chỉ file khác base + variant.patch)
│   ├── change-score-top-K/
│   ├── change-score-top-K-fibo/
│   ├── multi-pheromone/
│   ├── multi-pheromone-notoptpack/
│   ├── original-2phaseconnect-opt2/
│   └── original-fibo/
├── experiments-legacy/              # trích từ .claude/worktrees (nhánh 1908fix-opt)
│   ├── simple-aco/
│   ├── simple-aco-agent-opt2/
│   ├── city-based-item/
│   ├── current-best-algo/
│   ├── item-component/
│   ├── aco-optimized-by-antigravity/
│   └── aco-optimized-by-antigravity-save1/
├── experiments-2phaseconnect/       # từ exp-2phaseconnect (+ -ssh sau khi diff)
├── experiments-a280/                # từ experiment_setup_a280
├── experiments-esaco/               # từ ESACO-experiment (sau khi tách zip)
├── benchmark/                       # instances + benchmark dùng chung (dedupe)
├── results/                         # chỉ CSV/số liệu (không log/exe/sol)
├── scripts/                         # analyze_*.py, compare_*.py, run_experiment.py
├── reference/                       # EA/lkh3.py, LKH3 guide, paper-references
├── docs/                            # note.txt, improvement.md
├── Makefile                         # build base + overlay biến thể (make exp-<name>)
├── .gitignore                       # *.exe *.log *.sol __pycache__/ *.db *.zip
└── README.md                        # mô tả các biến thể + cách build/chạy
```

**Cơ chế biến thể:** mỗi `experiments/<tên>/` giữ (1) file source khác base ở đúng đường dẫn, (2) `variant.patch`, (3) `README.md` 1 dòng. `make exp-<tên>` = copy base → overlay → build.

## 5. Các bước thực thi (tuần tự, mỗi bước verify xong mới sang bước sau)

**B0. Snapshot an toàn (bắt buộc)**
- `git -C <từng thư mục> branch backup/pre-restructure` để giữ toàn bộ history.
- Không xóa gì trước bước này.

**B1. Khảo sát bổ sung (điểm còn mở)**
- Diff `exp-2phaseconnect` vs `exp-2phaseconnect-ssh` → xác định trùng hay khác.
- Xác định nội dung thật của `ESACO-experiment` và `experiment_setup_a280`.
- Verify `instances/` có trùng 100% giữa các thư mục (md5).

**B2. Di chuyển 2 zip** (2.2GB + 909MB) ra ổ ngoài.

**B3. Dựng repo mới**
- `git init` tại `acothop/`, base từ `original`, tạo `.gitignore`, `README.md`.

**B4. Trích biến thể**
- Diff `original` vs 6 thư mục còn lại → chép chỉ file khác + `variant.patch` vào `experiments/`.
- Trích `.claude/worktrees/1908fix-opt/src/*` → `experiments-legacy/` (bỏ `outputs/`, `*.exe`, log).
- Gập `exp-2phaseconnect`(+`-ssh`), `experiment_setup_a280`, `ESACO-experiment` vào các thư mục tương ứng.

**B5. Gộp tài nguyên chung**
- Dedupe `benchmark/` + `instances/` → 1 bản.
- Gom script phân tích → `scripts/`; note → `docs/`; EA + references → `reference/`.

**B6. Dọn rác**
- Xóa 9 thư mục trùng, `*.exe *.log *.sol __pycache__/ *.db patch_debug*.py`, root `main.cpp` trùng, `.git` lồng trong `benchmark/`.

**B7. Kiểm chứng & commit**
- Build lại `src/` từ base; build thử 1 biến thể qua `make exp-...`; xác nhận chạy được.
- Commit lần đầu.

## 6. Kiểm chứng cuối

- `git status` sạch, không còn file trùng/không theo dõi ngoài `.gitignore`.
- Build base + ≥1 biến thể chạy ra kết quả khớp (so với `results/` đã lưu).
- Tổng dung lượng ~1.5GB (so với ~25GB ban đầu).

## 7. Rủi ro & rollback

- **Không mất code:** mọi thư mục đã có `.git`; B0 thêm branch backup; zip vẫn còn ở ổ ngoài.
- Rollback bất kỳ bước nào = `git checkout backup/pre-restructure` ở thư mục gốc tương ứng.
- Chưa chạm `problemnote.txt` (note quan trọng của người dùng).
