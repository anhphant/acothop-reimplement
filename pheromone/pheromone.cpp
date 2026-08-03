#include <bits/stdc++.h>
using namespace std;

class Pheromone {
private:
    vector<vector<long double>> phe; // optimize this
public:
    void init(size_t n) {
        phe = vector<vector<long double>>(n, 
            vector<long double>(n, INF)); // max val
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
    long double getPheromone(int i, int j) {
        return phe[i][j]; // placeholder
    }
} Pheromone;