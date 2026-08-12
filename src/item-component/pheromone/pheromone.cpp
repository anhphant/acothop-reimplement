#include <bits/stdc++.h>
using namespace std;

class Pheromone {
private:
    vector<vector<long double>> phe;
    vector<vector<long double>> total;

public:
    void init(size_t n) {
        phe = vector<vector<long double>>(n, 
            vector<long double>(n, 10000000.0L)); // emulate MMAS trail_max
        total = vector<vector<long double>>(n, 
            vector<long double>(n, 0.0L));
    }
    void compute_total_information() {
        const long n = (long)phe.size();
        for (long i = 0; i < n; ++i) {
            for (long j = 0; j < n; ++j) {
                if (i == j) continue;
                long double d = connections.distance(componentList[i], componentList[j]);
                long double w = items[componentList[j].idItem].weight;
                long double p = items[componentList[j].idItem].profit;

                long double d_to_end = ::distance(componentList[j].idCity, nCities - 1);
                
                total[i][j] = pow(phe[i][j], params.alpha1) 
                                * pow(1.0L / (d + params.epsilon), params.alpha2)
                                * pow(1.0L / (w * (d_to_end + params.epsilon)), params.alpha3)
                                * pow(p, params.alpha4);
            }
        }
    }
    void pheromone_evaporation(){
        const long n = (long)phe.size();
        for (long i = 0; i < n; ++i)
            for (long j = 0; j < n; ++j)
                phe[i][j] *= (1.0L - params.rho);
    };

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
        long double trail_max = best_cost / params.rho;
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

    long double getTotal(int i, int j) {
        return total[i][j];
    }
} Pheromone;