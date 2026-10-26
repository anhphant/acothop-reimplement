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
            if (d == 0.0L) {
                heuristic[i][j] = 0.0L;
            } else {
                long double h = 1.0L;
                if (params.alpha2 == 1.0L) h = 1.0L / d;
                else if (params.alpha2 != 0.0L) h = pow(1.0L / d, params.alpha2);
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

    long double current_rho = 0.05L;

    // Dynamic Evaporation based on Stagnation
    // If the algorithm hasn't improved in many generations, rho is reset to rho_max to force exploration.
    // Otherwise, it progressively decays (cools down) to rho_min to exploit the current region.
    void dynamic_stagnation_evaporation(bool is_stagnating, long double rho_max, long double rho_min) {
        if (is_stagnating) {
            current_rho = rho_max; // Reset evaporation to explore
            // printf("[Dynamic] Pheromone reset due to stagnation!\n");
        } else {
            // Decay rho gradually to exploit
            current_rho *= 0.95L; 
            if (current_rho < rho_min) current_rho = rho_min;
        }

        const long n = (long)phe.size();
        for (long i = 0; i < n; ++i)
            for (long j = 0; j < n; ++j)
                phe[i][j] *= (1.0L - current_rho);
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