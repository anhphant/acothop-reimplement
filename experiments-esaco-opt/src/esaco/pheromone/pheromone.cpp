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
    // Lazy sparse store (exact equivalent of the old symmetric triangular
    // float matrix, which cost ~2.3 GB at n=33810). Every arc not in the
    // `nn` overlay is uniform and holds `base`; an arc is materialized into
    // the overlay with both directions only when it is individually touched
    // (nn-list evaporation, deposit, or ESACO S2 eviction). Uniform ops
    // (no-LS evaporation / trail-limit clamp) apply to `base` plus overlay.
    float base = 10000000.0f;
    double trail_0 = 10000000.0;   // initial pheromone level
    double trail_max = 10000000.0; // current MMAS upper trail limit
    double trail_min = 10000000.0; // current MMAS lower trail limit
    int n = 0;

    unordered_map<long long, pair<float, float>> nn;   // directed overlay for touched pairs

    static long long key(int i, int j) {
        return (i >= j) ? (long long)i * MAX_CITIES + j
                        : (long long)j * MAX_CITIES + i;
    }

    // Directed entry [i][j] of a pair in the overlay (key stores (min,max),
    // entry.first == [min][max], entry.second == [max][min]).
    static inline float& dir(int i, int j, pair<float, float>& e) {
        return (i < j) ? e.first : e.second;
    }
    static inline float dir(int i, int j, const pair<float, float>& e) {
        return (i < j) ? e.first : e.second;
    }

    // Directed value as in the old full matrix.
    float read_dir(int i, int j) const {
        auto it = nn.find(key(i, j));
        if (it != nn.end()) return dir(i, j, it->second);
        return base;
    }

public:
    void init(int n_, double initial_trail) {
        n = n_;
        trail_0 = initial_trail;
        trail_max = initial_trail;
        base = (float)initial_trail;
        for (int i = 0; i < n; ++i) {
            active_count[i] = 0;
        }
        nn.clear();
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
    // for each directed nn arc, copy phe[i][h] onto phe[h][i] when smaller.
    void compute_nn_list_total_information() {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < NN_ANTS; ++j) {
                int h = knn[i][j];
                auto it = nn.find(key(i, h));
                if (it == nn.end()) continue;
                float& d1 = dir(i, h, it->second);
                float& d2 = dir(h, i, it->second);
                if (d1 < d2) d2 = d1;
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
                    auto it = nn.find(key(i, j));
                    if (it == nn.end())
                        it = nn.emplace(key(i, j), make_pair(base, base)).first;
                    float& d = dir(i, j, it->second);
                    d *= (1.0 - params.rho);
                    if (d < trail_min) d = trail_min;
                }
        } else {
            base *= (1.0 - params.rho);
            for (auto& e : nn) {
                e.second.first *= (1.0 - params.rho);
                e.second.second *= (1.0 - params.rho);
            }
        }
    }

    void deposit_pheromone_on_the_visited_arc(int i, int j) {
        deposit_impl(i, j, params.delta);
        if (params.esaco3 || params.esaco2 || params.esaco) { add_active(i, j); add_active(j, i); }
    }
    void deposit_pheromone_on_the_visited_arc(int i, int j, double val) {
        deposit_impl(i, j, val);
        if (params.esaco3 || params.esaco2 || params.esaco) { add_active(i, j); add_active(j, i); }
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
        if (base > trail_max) base = (float)trail_max;
        if (base < trail_min) base = (float)trail_min;
        for (auto& e : nn) {
            if (e.second.first > trail_max) e.second.first = (float)trail_max;
            if (e.second.first < trail_min) e.second.first = (float)trail_min;
            if (e.second.second > trail_max) e.second.second = (float)trail_max;
            if (e.second.second < trail_min) e.second.second = (float)trail_min;
        }
    }
    void reset_pheromone() {
        // MMAS pheromone trail re-initialization (used on stagnation restart).
        // The old full-matrix reset wiped the directed state too.
        base = (float)trail_max;
        for (int i = 0; i < n; ++i) active_count[i] = 0;
        nn.clear();
    }
    double getPheromone(int i, int j) {
        return read_dir(i, j);
    }
    double getTotal(int i, int j) {
        return pow(read_dir(i, j), params.alpha1) * heuristic_ij(i, j);
    }
    // ESACO S1: neighbor j (city id) with max pheromone from i, real cities only.
    int argmax_pheromone(int i) const {
        if (i < 0 || i >= n) return -1;
        int best = -1;
        double best_val = -1.0;
        for (int k = 0; k < active_count[i]; ++k) {
            int j = active[i][k];
            double v = read_dir(i, j);
            if (v > best_val) { best_val = v; best = j; }
        }
        return best;
    }

    // ESACO S3: per-node list of arcs whose pheromone != trail_0 (the "sparse" view).
    int active[MAX_CITIES][MAX_KS];
    int active_count[MAX_CITIES];

    // Record arc (i,j) as deposited; drop the lowest-pheromone entry when full.
    // ESACO S2: a dropped (evicted) arc is forgotten — its pheromone reverts to
    // a value clamped to the current trail limits; the sparse list is authoritative.
    void add_active(int i, int j) {
        if (i < 0 || i >= nCities || j < 0 || j >= nCities) return;  // real cities only
        for (int k = 0; k < active_count[i]; ++k)
            if (active[i][k] == j) return;
        if (active_count[i] < MAX_KS) {
            active[i][active_count[i]++] = j;
        } else {
            int worst = 0;
            for (int k = 1; k < MAX_KS; ++k)
                if (read_dir(i, active[i][k]) < read_dir(i, active[i][worst])) worst = k;
            if (params.esaco2) {
                int evicted = active[i][worst];
                // reset to trail_0 clamped into [trail_min, trail_max]
                // could differ once trail limits move — see paper's sparse store.
                double reset_val = trail_0;
                if (reset_val > trail_max) reset_val = trail_max;
                if (reset_val < trail_min) reset_val = trail_min;
                auto it = nn.find(key(i, evicted));
                if (it != nn.end()) { it->second.first = it->second.second = (float)reset_val; }
                else nn.emplace(key(i, evicted), make_pair((float)reset_val, (float)reset_val));
                remove_active(evicted, i);
            }
            active[i][worst] = j;
        }
    }

    // ESACO S3/S2: drop arc (i,j) from i's active list (mirror cleanup after eviction).
    void remove_active(int i, int j) {
        if (i < 0 || i >= nCities) return;
        for (int k = 0; k < active_count[i]; ++k) {
            if (active[i][k] == j) {
                active[i][k] = active[i][active_count[i] - 1];
                active_count[i]--;
                return;
            }
        }
    }

    // On-the-fly transition value for non-nn arcs (tier-2): total[][] is only
    // precomputed for nn-list arcs, so compute directly from phe + heuristic.
    double get_total_direct(int i, int j) const {
        return pow(read_dir(i, j), params.alpha1) * heuristic_ij(i, j);
    }

private:
    // Old deposit: phe[i][j] += val (clamped to trail_max); phe[j][i] = phe[i][j].
    void deposit_impl(int i, int j, double val) {
        auto it = nn.find(key(i, j));
        if (it != nn.end()) {
            float nv = (float)((double)dir(i, j, it->second) + val);
            if (nv > trail_max) nv = (float)trail_max;
            it->second.first = it->second.second = nv;
        } else {
            float nv = (float)((double)base + val);
            if (nv > trail_max) nv = (float)trail_max;
            nn.emplace(key(i, j), make_pair(nv, nv));
        }
    }
} Pheromone;
