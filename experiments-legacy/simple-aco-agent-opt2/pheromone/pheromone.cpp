#include <bits/stdc++.h>
using namespace std;

// ============================================================
// Pheromone với Sparse Tracking (ESACO Strategies 1 & 3)
// ------------------------------------------------------------
// Lưu full matrix tau[n][n] như MMAS chuẩn cho correctness.
// Bổ sung sparse tracking: với mỗi node i, track các arc (i,j)
// đã được deposit (tau[i][j] > tau0), phục vụ:
//   - Strategy 1: get_best_pheromone_neighbor(i) = argmax tau[i][j]
//   - Strategy 3: get_sparse_neighbors(i) = các j đã được chọn
// ============================================================

class Pheromone {
private:
    vector<vector<long double>> phe;
    vector<vector<long double>> total;

    // Sparse tracking: với mỗi node i, lưu set các j đã deposit
    // Đây chỉ là index tracking, không lưu giá trị (giá trị trong phe[][])
    vector<unordered_set<int>> deposited_arcs;

    long double tau0; // initial pheromone value

    static constexpr int MAX_SPARSE_TRACK = 50; // giới hạn tracking list

public:
    void init(size_t n) {
        tau0 = 10000000.0L; // emulate MMAS trail_max at init
        phe = vector<vector<long double>>(n, vector<long double>(n, tau0));
        total = vector<vector<long double>>(n, vector<long double>(n, 0.0L));
        deposited_arcs.assign(n, unordered_set<int>());
    }

    void compute_total_information() {
        const long n = (long)phe.size();
        for (long i = 0; i < n; ++i) {
            for (long j = 0; j < n; ++j) {
                if (i == j) continue;
                long double d = connections.distance(componentList[i], componentList[j]);
                if (d > 0.0L) {
                    total[i][j] = pow(phe[i][j], params.alpha) * pow(1.0L / d, params.beta);
                } else {
                    total[i][j] = 0.0L;
                }
            }
        }
    }

    void pheromone_evaporation() {
        const long n = (long)phe.size();
        for (long i = 0; i < n; ++i)
            for (long j = 0; j < n; ++j)
                phe[i][j] *= (1.0L - params.rho);
    }

    void deposit_pheromone_on_the_visited_arc(int i, int j) {
        phe[i][j] += params.delta;
        phe[j][i] = phe[i][j];
        _track(i, j);
    }

    void deposit_pheromone_on_the_visited_arc(int i, int j, long double val) {
        phe[i][j] += val;
        phe[j][i] = phe[i][j];
        _track(i, j);
    }

    void check_pheromone_trail_limits(long double best_cost) {
        if (best_cost <= 0) return;
        const long n = (long)phe.size();
        long double trail_max = best_cost / params.rho;
        long double trail_min = trail_max / (2.0L * n);
        for (long i = 0; i < n; ++i) {
            for (long j = 0; j < n; ++j) {
                if (phe[i][j] > trail_max) phe[i][j] = trail_max;
                if (phe[i][j] < trail_min) phe[i][j] = trail_min;
            }
        }
    }

    long double getPheromone(int i, int j) const { return phe[i][j]; }
    long double getTotal(int i, int j) const { return total[i][j]; }

    // --------------------------------------------------------
    // ESACO Strategy 1 helper: node j có pheromone cao nhất từ i
    // --------------------------------------------------------
    int get_best_pheromone_neighbor(int i) const {
        if (deposited_arcs[i].empty()) return -1;
        int best_j = -1;
        long double best_tau = -1.0L;
        for (int j : deposited_arcs[i]) {
            if (phe[i][j] > best_tau) {
                best_tau = phe[i][j];
                best_j = j;
            }
        }
        return best_j;
    }

    // --------------------------------------------------------
    // ESACO Strategy 3 helper: các node đã từng được chọn từ i
    // --------------------------------------------------------
    vector<int> get_sparse_neighbors(int i) const {
        return vector<int>(deposited_arcs[i].begin(), deposited_arcs[i].end());
    }

private:
    // Track các arc được deposit (giới hạn MAX_SPARSE_TRACK per node)
    void _track(int i, int j) {
        if (deposited_arcs[i].size() < (size_t)MAX_SPARSE_TRACK) {
            deposited_arcs[i].insert(j);
        }
        if (deposited_arcs[j].size() < (size_t)MAX_SPARSE_TRACK) {
            deposited_arcs[j].insert(i);
        }
    }
} Pheromone;