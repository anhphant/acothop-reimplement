#include <bits/stdc++.h>
using namespace std;

int num_subpheromones = 1;

class ParentPheromoneClass {
private:
    double phe[MAX_CITIES][MAX_PARENT_KN];
    double trail_0 = 10000000.0;   // initial pheromone level
    double trail_max = 10000000.0; // current MMAS upper trail limit
    double trail_min = 10000000.0; // current MMAS lower trail limit
    int n = 0;
public:
    void init(int n_, double initial_trail) {
        n = n_;
        trail_0 = initial_trail;
        trail_max = initial_trail;
        trail_min = initial_trail / (2.0 * n);
        for (int i = 0; i < n; ++i) {
            for (int k = 0; k < NN_PARENT_ANTS; ++k) {
                phe[i][k] = trail_0;
            }
        }
    }

    void pheromone_evaporation(){
        if (params.local_search_flag > 0) {
            for (int i = 0; i < n; ++i) {
                for (int r = 0; r < NN_ANTS; ++r) {
                    int j = knn[i][r];
                    for (int k = 0; k < NN_PARENT_ANTS; ++k) {
                        if (parent_knn[i][k] == j) {
                            phe[i][k] *= (1.0 - params.rho);
                            if (phe[i][k] < trail_min) phe[i][k] = trail_min;
                            break;
                        }
                    }
                }
            }
        } else {
            for (int i = 0; i < n; ++i) {
                for (int k = 0; k < NN_PARENT_ANTS; ++k) {
                    phe[i][k] *= (1.0 - params.rho);
                    if (phe[i][k] < trail_min) phe[i][k] = trail_min;
                }
            }
        }
    }
    void deposit_pheromone_on_the_visited_arc(int i, int j, double val) {
        for (int k = 0; k < NN_PARENT_ANTS; ++k) {
            if (parent_knn[i][k] == j) {
                phe[i][k] += val;
                break;
            }
        }
        for (int k = 0; k < NN_PARENT_ANTS; ++k) {
            if (parent_knn[j][k] == i) {
                phe[j][k] += val;
                break;
            }
        }
    }
    void update_trail_limits(double fitness) {
        if (fitness <= 0) return;
        trail_max = 1.0 / (params.rho * fitness);
        if (params.local_search_flag > 0) {
            trail_min = trail_max / (2.0 * n);
        } else {
            double p_x = exp(log(0.05) / n);
            trail_min = 1.0 * (1.0 - p_x) / (p_x * (double)((NN_ANTS + 1) / 2));
            trail_min = trail_max * trail_min;
        }
    }
    void check_pheromone_trail_limits(double fitness) {
        update_trail_limits(fitness);
        for (int i = 0; i < n; ++i) {
            for (int k = 0; k < NN_PARENT_ANTS; ++k) {
                if (phe[i][k] > trail_max) phe[i][k] = trail_max;
                if (phe[i][k] < trail_min) phe[i][k] = trail_min;
            }
        }
    }
    void reset_pheromone() {
        for (int i = 0; i < n; ++i)
            for (int k = 0; k < NN_PARENT_ANTS; ++k)
                phe[i][k] = trail_max;
    }
    double getPheromone(int i, int j) const {
        for (int k = 0; k < NN_PARENT_ANTS; ++k) {
            if (parent_knn[i][k] == j) return phe[i][k];
        }
        return trail_min;
    }
    double getTrailMin() const { return trail_min; }
    double getTrailMax() const { return trail_max; }
    double getTrail0() const { return trail_0; }
} ParentPheromone;


class SubPheromoneClass {
private:
    double phe[MAX_CITIES][MAX_KN];
    double total[MAX_CITIES][MAX_KN];
    double heuristic[MAX_CITIES][MAX_KN];
    int n = 0;
public:
    double b1, b2, b3;

    void init(int n_, double b1_, double b2_, double b3_) {
        n = n_;
        b1 = b1_;
        b2 = b2_;
        b3 = b3_;
        for (int i = 0; i < n; ++i) {
            for (int k = 0; k < NN_ANTS; ++k) {
                phe[i][k] = ParentPheromone.getTrail0();
                total[i][k] = 0.0;
            }
        }
    }

    void compute_heuristic() {
        for (int i = 0; i < n; ++i) {
            for (int k = 0; k < NN_ANTS; ++k) {
                int j = knn[i][k];
                if (i == j) { heuristic[i][k] = 0.0; continue; }
                double d = connections.distance(componentList[i], componentList[j]);
                long int rounded_d = (long int)(d + 0.5);
                double w = city_weight_topk[j];
                double d_profit = city_profit_topk[j];
                double profit_factor = (d_profit > 0.0) ? pow(d_profit, b2) : 1.0;
                heuristic[i][k] = pow(1.0 / (rounded_d + params.eps1), b1) *
                                   pow(1.0 / (w + params.eps2), b3) *
                                   profit_factor;
            }
        }
    }

    void compute_total_information() {
        for (int i = 0; i < n; ++i) {
            for (int k = 0; k < NN_ANTS; ++k) {
                int j = knn[i][k];
                if (i == j) continue;
                total[i][k] = pow(ParentPheromone.getPheromone(i, j), params.alpha1) * 
                              pow(phe[i][k], params.alpha2) * 
                              heuristic[i][k];
            }
        }
    }

    void compute_nn_list_total_information() {
        for (int i = 0; i < n; ++i) {
            for (int k = 0; k < NN_ANTS; ++k) {
                int h = knn[i][k];
                // Force symmetry for SubPheromone
                for(int r = 0; r < NN_ANTS; ++r) {
                    if (knn[h][r] == i) {
                        if (phe[i][k] < phe[h][r]) phe[h][r] = phe[i][k];
                        else phe[i][k] = phe[h][r];
                        break;
                    }
                }
                total[i][k] = pow(ParentPheromone.getPheromone(i, h), params.alpha1) * 
                              pow(phe[i][k], params.alpha2) * 
                              heuristic[i][k];
            }
        }
    }

    void pheromone_evaporation() {
        double trail_min = ParentPheromone.getTrailMin();
        double trail_max = ParentPheromone.getTrailMax();
        for (int i = 0; i < n; ++i) {
            for (int k = 0; k < NN_ANTS; ++k) {
                phe[i][k] *= (1.0 - params.rho);
                if (phe[i][k] < trail_min) phe[i][k] = trail_min;
                if (phe[i][k] > trail_max) phe[i][k] = trail_max;
            }
        }
    }

    void deposit_pheromone_on_the_visited_arc(int i, int j, double val) {
        for (int k = 0; k < NN_ANTS; ++k) {
            if (knn[i][k] == j) {
                phe[i][k] += val;
                break;
            }
        }
        for (int k = 0; k < NN_ANTS; ++k) {
            if (knn[j][k] == i) {
                phe[j][k] += val;
                break;
            }
        }
    }

    void reset_pheromone() {
        double trail_max = ParentPheromone.getTrailMax();
        for (int i = 0; i < n; ++i)
            for (int k = 0; k < NN_ANTS; ++k)
                phe[i][k] = trail_max;
    }

    double getPheromone(int i, int j) {
        for (int k = 0; k < NN_ANTS; ++k) {
            if (knn[i][k] == j) return phe[i][k];
        }
        return ParentPheromone.getTrailMin();
    }

    double getTotal(int i, int j) {
        for (int k = 0; k < NN_ANTS; ++k) {
            if (knn[i][k] == j) return total[i][k];
        }
        if (i == j) return 0.0;
        // Ultra-fast fallback for non-NN edges to prevent massive CPU overhead
        // from calling pow() millions of times in choose_best_next().
        double d = connections.distance(componentList[i], componentList[j]);
        return ParentPheromone.getPheromone(i, j) / (d + params.eps1);
    }

    double getTotal_nn(int i, int k) {
        return total[i][k];
    }
};

vector<SubPheromoneClass> SubPheromones;

void initSubPheromones(int n) {
    SubPheromones.clear();
    for (int i = -params.cnt1; i <= params.cnt1; ++i) {
        double b1 = params.beta1 + i * params.sigma1;
        for (int j = -params.cnt2; j <= params.cnt2; ++j) {
            double b2 = params.beta2 + j * params.sigma2;
            for (int k = -params.cnt3; k <= params.cnt3; ++k) {
                double b3 = params.beta3 + k * params.sigma3;
                SubPheromoneClass sp;
                sp.init(n, b1, b2, b3);
                SubPheromones.push_back(sp);
            }
        }
    }
    num_subpheromones = SubPheromones.size();
}

void compute_all_heuristics() {
    for (auto& sp : SubPheromones) sp.compute_heuristic();
}

void compute_all_total_information() {
    for (auto& sp : SubPheromones) sp.compute_total_information();
}

void compute_all_nn_list_total_information() {
    for (auto& sp : SubPheromones) sp.compute_nn_list_total_information();
}

void all_pheromone_evaporation() {
    ParentPheromone.pheromone_evaporation();
    for (auto& sp : SubPheromones) sp.pheromone_evaporation();
}

void all_reset_pheromone() {
    ParentPheromone.reset_pheromone();
    for (auto& sp : SubPheromones) sp.reset_pheromone();
}
