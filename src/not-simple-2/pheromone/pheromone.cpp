#include <bits/stdc++.h>
using namespace std;

// Precomputed static heuristic factor: (1/(dist+eps1))^alpha2 * (1/(w+eps2))^alpha3 * profit^alpha4
// This does NOT change across iterations — only pheromone does.
// Recomputed once via Pheromone.compute_heuristic() at startup.
double heuristic[MAX_CITIES][MAX_CITIES];

class Pheromone {
private:
    double phe[MAX_CITIES][MAX_CITIES];
    double total[MAX_CITIES][MAX_CITIES];
    double trail_0 = 10000000.0;   // initial pheromone level
    double trail_max = 10000000.0; // current MMAS upper trail limit
    double trail_min = 10000000.0; // current MMAS lower trail limit
    int n = 0;
public:
    void init(int n_, double initial_trail) {
        n = n_;
        trail_0 = initial_trail;
        trail_max = initial_trail;
#if 0
        cerr << "PHEROMONE|trail_0=" << scientific << setprecision(15) << trail_0 << "\n";
#endif
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                phe[i][j] = trail_0;
                total[i][j] = 0.0;
            }
        }
    }

    // Call once after params and problem are loaded.
    // Precomputes the static (distance + item) part of the heuristic.
    void compute_heuristic() {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (i == j) { heuristic[i][j] = 0.0; continue; }
                double d = connections.distance(componentList[i], componentList[j]);
                long int rounded_d = (long int)(d + 0.5);
                double w = city_weight[j];
                double d_profit = city_profit[j];
                double profit_factor = (d_profit > 0.0) ? pow(d_profit, params.alpha4) : 1.0;
                heuristic[i][j] = pow(1.0 / (rounded_d + params.eps1), params.alpha2) *
                                   pow(1.0 / (w + params.eps2), params.alpha3) *
                                   profit_factor;
            }
        }
    }

    void compute_total_information() {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                if (i == j) continue;
                total[i][j] = pow(phe[i][j], params.alpha1) * heuristic[i][j];
            }
        }
    }
    // realbench compute_nn_list_total_information (ants.c:240), used when LS is on:
    // forces pheromone symmetry on nn_list arcs and computes total only there.
    void compute_nn_list_total_information() {
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < NN_ANTS; ++j) {
                int h = knn[i][j];
                if (phe[i][h] < phe[h][i])
                    phe[h][i] = phe[i][h];
                total[i][h] = pow(phe[i][h], params.alpha1) * heuristic[i][h];
                total[h][i] = total[i][h];
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
    }
    void deposit_pheromone_on_the_visited_arc(int i, int j, double val) {
        phe[i][j] += val;
        phe[j][i] = phe[i][j];
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
    }
    double getPheromone(int i, int j) {
        return phe[i][j];
    }
    double getTotal(int i, int j) {
        return total[i][j];
    }
} Pheromone;

