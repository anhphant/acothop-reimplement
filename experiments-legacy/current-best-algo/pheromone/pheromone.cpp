#include <bits/stdc++.h>
using namespace std;

class Pheromone {
private:
    vector<vector<long double>> phe; 
    vector<vector<long double>> heuristic;
public:
    void init(size_t n) {
        phe = vector<vector<long double>>(n, 
            vector<long double>(n, 10000000.0L)); // emulate MMAS trail_max
        heuristic = vector<vector<long double>>(n, 
            vector<long double>(n, -1.0L)); // -1.0L means uninitialized
    }

    long double getHeuristic(int i, int j) {
        if (heuristic[i][j] < -0.5L) {
            long double d = connections.distance(componentList[i], componentList[j]);
            long double w = max((long double)componentList[j].totalWeight, 1.0L);
            long double p = max((long double)componentList[j].totalProfit, 1.0L);
            if (d == 0.0L) {
                heuristic[i][j] = 0.0L;
            } else {
                long double h = pow(1.0L / d, params.alpha2);
                if (params.alpha3 != 0) h *= pow(p, params.alpha3);
                if (params.alpha4 != 0) h *= pow(1.0L / w, params.alpha4);
                heuristic[i][j] = h;
            }
        }
        return heuristic[i][j];
    }

    long double getTotal(int i, int j) {
        if (params.alpha1 == 1.0L) {
            return phe[i][j] * getHeuristic(i, j);
        } else {
            return pow(phe[i][j], params.alpha1) * getHeuristic(i, j);
        }
    }


    void compute_total_information() {
        // No longer needed, total information is computed lazily on the fly!
    }
    void pheromone_evaporation(){
        const long n = (long)phe.size();
        for (long i = 0; i < n; ++i)
            for (long j = 0; j < n; ++j)
                phe[i][j] *= (1.0L - params.rho);
    };

    template <typename AntList>
    void adaptive_pheromone_evaporation(const AntList& ants, long double rho_min = 0.05L, long double rho_max = 0.63L) {
        const long n = (long)phe.size();
        const long n_ants = (long)ants.size();
        if (n <= 1 || n_ants <= 0) return;

        unordered_map<long long, int> edge_count;
        long long total_edges = 0;

        for (const auto& ant : ants) {
            const auto& seq = ant.state.sequence;
            if (seq.size() < 2) continue;
            for (size_t k = 0; k + 1 < seq.size(); ++k) {
                int u = seq[k].id;
                int v = seq[k + 1].id;
                if (u > v) swap(u, v);
                long long edge_key = ((long long)u << 32) | (long long)v;
                edge_count[edge_key]++;
                total_edges++;
            }
        }

        if (total_edges == 0) return;

        double H = 0.0;
        for (const auto& kv : edge_count) {
            double p_ij = (double)kv.second / (double)total_edges;
            if (p_ij > 1e-15) {
                H -= p_ij * log2(p_ij);
            }
        }

        double H_min = log2((double)n);
        double H_max = log2((double)(n * n_ants));

        double ratio = 0.0;
        if (H_max > H_min + 1e-12) {
            ratio = (H - H_min) / (H_max - H_min);
            if (ratio < 0.0) ratio = 0.0;
            if (ratio > 1.0) ratio = 1.0;
        }

        long double adaptive_rho = rho_min + (rho_max - rho_min) * ratio;

        for (long i = 0; i < n; ++i)
            for (long j = 0; j < n; ++j)
                phe[i][j] *= (1.0L - adaptive_rho);
    }
    void deposit_pheromone_on_the_visited_arc(int i, int j) {
        phe[i][j] += params.delta;
        phe[j][i] = phe[i][j];
    }
    void deposit_pheromone_on_the_visited_arc(int i, int j, long double val) {
        phe[i][j] += val;
        phe[j][i] = phe[i][j];
    }
    void check_pheromone_trail_limits(long double best_cost) {
        if (best_cost <= 0) return;
        const long n = (long)phe.size();
        long double trail_max = (1.0L / best_cost) / params.rho;
        long double trail_min = trail_max / (2.0L * n);
        for (long i = 0; i < n; ++i) {
            for (long j = 0; j < n; ++j) {
                if (phe[i][j] > trail_max) phe[i][j] = trail_max;
                if (phe[i][j] < trail_min) phe[i][j] = trail_min;
            }
        }
    }
    long double getPheromone(int i, int j) {
        return phe[i][j]; 
    }
} Pheromone;