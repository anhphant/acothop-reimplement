#include <bits/stdc++.h>
using namespace std;

// Heuristic is computed on the fly (no O(n^2) matrix). The item part is
// precomputed per city once; the distance part uses distance() directly.
double city_factor[MAX_CITIES];

double heuristic_ij(int i, int j) {
    if (i == j) return 0.0;
    long int rounded_d = (long int)(distance(i, j) + 0.5);
    return pow(1.0 / ((double)rounded_d + params.eps1), params.alpha2) * city_factor[j];
}

class Pheromone {
private:
    float* phe[MAX_CITIES];   // heap-allocated rows; float halves memory for large n
    double trail_0 = 10000000.0;   // initial pheromone level
    double trail_max = 10000000.0; // current MMAS upper trail limit
    double trail_min = 10000000.0; // current MMAS lower trail limit
    int n = 0;
public:
    void init(int n_, double initial_trail) {
        n = n_;
        trail_0 = initial_trail;
        trail_max = initial_trail;
        for (int i = 0; i < n; ++i) {
            phe[i] = new float[n];
            for (int j = 0; j < n; ++j)
                phe[i][j] = (float)trail_0;
            active_count[i] = 0;
        }
    }

    // Call once after params and problem are loaded.
    // Precomputes the static (distance + item) part of the heuristic.
    void compute_heuristic() {
        for (int j = 0; j < n; ++j) {
            double w = city_weight_topk[j];
            double d_profit = city_profit_topk[j];
            double profit_factor = (params.alpha4 > 0.0 && d_profit > 0.0) ? pow(d_profit, params.alpha4) : 1.0;
            double weight_factor = (params.alpha3 > 0.0) ? pow(1.0 / (w + params.eps2), params.alpha3) : 1.0;
            city_factor[j] = weight_factor * profit_factor;
        }
    }

    void compute_total_information() {
        // total is computed on the fly in getTotal(); nothing to precompute.
    }
    // realbench compute_nn_list_total_information (ants.c:240), used when LS is on:
    // forces pheromone symmetry on nn_list arcs and computes total only there.
    void compute_nn_list_total_information() {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < NN_ANTS; ++j) {
                int h = knn[i][j];
                if (phe[i][h] < phe[h][i])
                    phe[h][i] = phe[i][h];
            }
        }
    }
    void pheromone_evaporation(){
        if (params.local_search_flag > 0) {
            // realbench mmas_evaporation_nn_list: evaporate candidate-list arcs
            // and clamp to trail_min (only lower bound, no trail_max check).
            for (int i = 0; i < n; ++i)
                for (int k = 0; k < NN_ANTS; ++k) {
                    int j = knn[i][k];
                    phe[i][j] *= (1.0 - params.rho);
                    if (phe[i][j] < trail_min) phe[i][j] = trail_min;
                }
        } else {
            for (int i = 0; i < n; ++i)
                for (int j = 0; j < n; ++j)
                    phe[i][j] *= (1.0 - params.rho);
        }
    };
    void deposit_pheromone_on_the_visited_arc(int i, int j) {
        phe[i][j] += params.delta;
        phe[j][i] = phe[i][j];
        if (params.esaco3 || params.esaco2) { add_active(i, j); add_active(j, i); }
    }
    void deposit_pheromone_on_the_visited_arc(int i, int j, double val) {
        phe[i][j] += val;
        phe[j][i] = phe[i][j];
        if (params.esaco3 || params.esaco2) { add_active(i, j); add_active(j, i); }
    }
    void update_trail_limits(double fitness) {
        if (fitness <= 0) return;
        trail_max = 1.0 / (params.rho * fitness);
        if (params.local_search_flag > 0) {
            // realbench LS case: trail_min = trail_max / (2*n)
            trail_min = trail_max / (2.0 * n);
        } else {
            // realbench no-LS case (ants.c update_statistics): p_x formula.
            // Note (NN_ANTS+1)/2 is INTEGER division, matching realbench.
            double p_x = exp(log(0.05) / n);
            trail_min = 1.0 * (1.0 - p_x) / (p_x * (double)((NN_ANTS + 1) / 2));
            trail_min = trail_max * trail_min;
        }
    }
    void check_pheromone_trail_limits(double fitness) {
        update_trail_limits(fitness);
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (phe[i][j] > trail_max) phe[i][j] = trail_max;
                if (phe[i][j] < trail_min) phe[i][j] = trail_min;
            }
        }
    }
    void reset_pheromone() {
        // MMAS pheromone trail re-initialization (used on stagnation restart)
        for (int i = 0; i < n; ++i)
            for (int j = 0; j < n; ++j)
                phe[i][j] = trail_max;
        for (int i = 0; i < n; ++i) active_count[i] = 0;
    }
    double getPheromone(int i, int j) {
        return phe[i][j];
    }
    double getTotal(int i, int j) {
        return pow(phe[i][j], params.alpha1) * heuristic_ij(i, j);
    }
    // ESACO S1: neighbor j (city id) with max pheromone from i, real cities only.
    int argmax_pheromone(int i) const {
        if (i < 0 || i >= n || phe[i] == nullptr) return -1;   // phe not allocated yet (pre-init)
        int best = -1;
        double best_val = -1.0;
        for (int j = 0; j < nCities; ++j) {
            if (j == i) continue;
            if (phe[i][j] > best_val) { best_val = phe[i][j]; best = j; }
        }
        return best;
    }

    // ESACO S3: per-node list of arcs whose pheromone != trail_0 (the "sparse" view).
    int active[MAX_CITIES][MAX_KS];
    int active_count[MAX_CITIES];

    // Record arc (i,j) as deposited; drop the lowest-pheromone entry when full.
    // ESACO S2: a dropped (evicted) arc is forgotten — its pheromone reverts to
    // trail_0, so the sparse list is the authoritative store, not just an index.
    void add_active(int i, int j) {
        if (i < 0 || i >= nCities || j < 0 || j >= nCities) return;  // real cities only
        for (int k = 0; k < active_count[i]; ++k)
            if (active[i][k] == j) return;
        if (active_count[i] < MAX_KS) {
            active[i][active_count[i]++] = j;
        } else {
            int worst = 0;
            for (int k = 1; k < MAX_KS; ++k)
                if (phe[i][active[i][k]] < phe[i][active[i][worst]]) worst = k;
            if (params.esaco2) {
                int evicted = active[i][worst];
                // ponytail: reset to trail_0 (initial tau_0); a post-update "default"
                // could differ once trail limits move — see paper's sparse store.
                phe[i][evicted] = trail_0;
                phe[evicted][i] = trail_0;
            }
            active[i][worst] = j;
        }
    }

    // On-the-fly transition value for non-nn arcs (tier-2): total[][] is only
    // precomputed for nn-list arcs, so compute directly from phe + heuristic.
    double get_total_direct(int i, int j) const {
        return pow(phe[i][j], params.alpha1) * heuristic_ij(i, j);
    }
} Pheromone;

