#include <bits/stdc++.h>
using namespace std;

class Pheromone {
private:
    struct EdgeInfo {
        long double tau = INF;
        int last_update = 0;
    };
    vector<unordered_map<int, EdgeInfo>> phe;
public:
    int current_generation = 0;

    void init(size_t n) {
        phe.clear();
        phe.resize(n);
        current_generation = 0;
    }

    void pheromone_evaporation() {
        current_generation++; // Lazy update: just increment time!
    }

    long double tau_max = 1000.0L;
    long double tau_min = 0.5L;

    void reset_all() {
        for (auto& row : phe) row.clear();
        current_generation = 0;
    }

    void deposit_pheromone_on_the_visited_arc(int i, int j) {
        long double tau_i = getPheromone(i, j);
        long double new_tau = tau_i + params.delta;
        phe[i][j] = {max(tau_min, min(tau_max, new_tau)), current_generation};
        phe[j][i] = phe[i][j];
    }

    long double getPheromone(int i, int j) {
        auto it = phe[i].find(j);
        if (it == phe[i].end()) return tau_max; // initial value

        long double tau = it->second.tau;
        int elapsed = current_generation - it->second.last_update;
        if (elapsed > 0) {
            tau *= pow(1.0L - params.rho, elapsed);
        }
        return max(tau_min, min(tau_max, tau));
    }
} Pheromone;