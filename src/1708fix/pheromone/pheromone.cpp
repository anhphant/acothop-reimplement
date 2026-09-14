#include <bits/stdc++.h>
using namespace std;

class Pheromone {
private:
    long double phe[MAX_CITIES][64]; 
    long double heuristic[MAX_CITIES][64];
    long double trail_min_global = 0.0001L;
    long double trail_max_global = 10000000.0L;

public:
    void init(size_t n) {
        for (int i = 0; i < n; ++i) {
            for (int k = 0; k < connections.nn_list_len; ++k) {
                phe[i][k] = 10000000.0L; // emulate MMAS trail_max
                
                int j = connections.nn_list[i][k];
                if (j != -1) {
                    long double d = connections.distance(componentList[i], componentList[j]);
                    long double w = max((long double)componentList[j].totalWeight, 1.0L);
                    long double p = max((long double)componentList[j].totalProfit, 1.0L);
                    if (d == 0.0L) {
                        heuristic[i][k] = 0.0L;
                    } else {
                        long double h = pow(1.0L / d, params.alpha2);
                        if (params.alpha3 != 0) h *= pow(p, params.alpha3);
                        if (params.alpha4 != 0) h *= pow(1.0L / w, params.alpha4);
                        heuristic[i][k] = h;
                    }
                } else {
                    heuristic[i][k] = -1.0L;
                }
            }
        }
    }

    long double getHeuristic(int i, int j) {
        for (int k = 0; k < connections.nn_list_len; ++k) {
            if (connections.nn_list[i][k] == j) return heuristic[i][k];
        }
        // Fallback for non-candidate edges
        long double d = connections.distance(componentList[i], componentList[j]);
        long double w = max((long double)componentList[j].totalWeight, 1.0L);
        long double p = max((long double)componentList[j].totalProfit, 1.0L);
        if (d == 0.0L) return 0.0L;
        long double h = pow(1.0L / d, params.alpha2);
        if (params.alpha3 != 0) h *= pow(p, params.alpha3);
        if (params.alpha4 != 0) h *= pow(1.0L / w, params.alpha4);
        return h;
    }

    long double getTotal(int i, int j) {
        long double p_val = trail_min_global;
        for (int k = 0; k < connections.nn_list_len; ++k) {
            if (connections.nn_list[i][k] == j) {
                p_val = phe[i][k];
                break;
            }
        }
        if (params.alpha1 == 1.0L) {
            return p_val * getHeuristic(i, j);
        } else {
            return pow(p_val, params.alpha1) * getHeuristic(i, j);
        }
    }

    long double tau(int i, int j) const {
        for (int k = 0; k < connections.nn_list_len; ++k) {
            if (connections.nn_list[i][k] == j) return phe[i][k];
        }
        return trail_min_global;
    }

    void evaporate(long double rho) {
        for (long i = 0; i < numComponents; ++i)
            for (int k = 0; k < connections.nn_list_len; ++k)
                phe[i][k] *= (1.0L - rho);
    }

    void deposit_pheromone_on_the_visited_arc(int i, int j, long double val) {
        for (int k = 0; k < connections.nn_list_len; ++k) {
            if (connections.nn_list[i][k] == j) { phe[i][k] += val; break; }
        }
        for (int k = 0; k < connections.nn_list_len; ++k) {
            if (connections.nn_list[j][k] == i) { phe[j][k] += val; break; }
        }
    }

    void deposit_pheromone_on_the_visited_arc(int i, int j) {
        deposit_pheromone_on_the_visited_arc(i, j, params.delta);
    }

    void check_pheromone_trail_limits(long double best_cost) {
        if (best_cost <= 0) return;
        long double trail_max = (1.0L / best_cost) / params.rho;
        long double trail_min = trail_max / (2.0L * numComponents);
        trail_min_global = trail_min;
        trail_max_global = trail_max;
        
        for (long i = 0; i < numComponents; ++i) {
            for (int k = 0; k < connections.nn_list_len; ++k) {
                if (phe[i][k] > trail_max) phe[i][k] = trail_max;
                if (phe[i][k] < trail_min) phe[i][k] = trail_min;
            }
        }
    }

    long double getPheromone(int i, int j) {
        for (int k = 0; k < connections.nn_list_len; ++k) {
            if (connections.nn_list[i][k] == j) return phe[i][k];
        }
        return trail_min_global;
    }
} Pheromone;