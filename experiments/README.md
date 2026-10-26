# Biến thể thuật toán chính

Mỗi thư mục chứa **chỉ các file khác so với base** (`src/not-simple-2-tuneopt/`) + `variant.patch`. Build: overlay file biến thể lên base rồi biên dịch.

| Biến thể | File khác base |
|---|---|
| `change-score-top-K` | aco.cpp, daemon_actions/optional.cpp, helper/helper.cpp, main.cpp, packing/packing.cpp, pheromone/pheromone.cpp, problem/thopproblem.cpp, run.ps1 |
| `change-score-top-K-fibo` | aco.cpp, colony/ant.cpp, colony/colony.cpp, daemon_actions/optional.cpp, helper/helper.cpp, main.cpp, packing/packing.cpp, pheromone/pheromone.cpp, problem/thopproblem.cpp, run.ps1 |
| `multi-pheromone` | aco.cpp, colony/ant.cpp, colony/colony.cpp, daemon_actions/optional.cpp, helper/helper.cpp, main.cpp, packing/packing.cpp, pheromone/pheromone.cpp, problem/thopproblem.cpp, run.ps1 + run_multi_pheromone.ps1, test.cpp, test_profit.cpp |
| `multi-pheromone-notoptpack` | (như multi-pheromone) |
| `original-2phaseconnect-opt2` | aco.cpp, colony/ant.cpp, colony/colony.cpp, daemon_actions/optional.cpp, helper/helper.cpp, ls/ls.cpp, main.cpp, packing/packing.cpp, pheromone/pheromone.cpp, problem/thopproblem.cpp, run.ps1 + check_regression.ps1, walkthrough.md |
| `original-fibo` | aco.cpp, colony/ant.cpp, colony/colony.cpp, daemon_actions/optional.cpp, helper/helper.cpp, main.cpp, problem/thopproblem.cpp, run.ps1 |

Ghi chú: các thư mục gốc đã bị xóa trong quá trình tái cấu trúc (trùng lặp): `change-score-top-K-stratified` = change-score-top-K, `original-2phaseconnect` = original, `original-2phaseconnect - Copy` = opt2, `original-fibofull` = fibo.
