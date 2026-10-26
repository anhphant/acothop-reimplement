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

    void deposit_pheromone_on_the_visited_arc(int i, int j) {
        // Apply lazy evaporation before adding new pheromone
        long double tau_i = getPheromone(i, j);
        phe[i][j] = {tau_i + params.delta, current_generation};
        phe[j][i] = phe[i][j];
    }

    long double getPheromone(int i, int j) {
        auto it = phe[i].find(j);
        if (it == phe[i].end()) return INF; // initial value

        long double tau = it->second.tau;
        int elapsed = current_generation - it->second.last_update;
        if (elapsed > 0) {
            tau *= pow(1.0L - params.rho, elapsed);
        }
        return tau;
    }
} Pheromone;