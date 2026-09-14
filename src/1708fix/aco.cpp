#include <bits/stdc++.h>
using namespace std;

struct Component {
    int id;
    int x, y;
    double totalWeight = 0.0; 
    double totalProfit = 0.0;
};
Component componentList[MAX_CITIES];
int numComponents = 0;

struct Connections {
    mutable vector<vector<long double>> dist_matrix;
    int nn_list[MAX_CITIES][64];
    int nn_list_len = 64;
    
    void init(int max_id) {
        dist_matrix.assign(max_id + 1, vector<long double>(max_id + 1, -1.0));
        
        // Precompute 64 nearest neighbors for FACO
        for (int i = 0; i <= max_id; ++i) {
            vector<pair<long double, int>> neighbors;
            for (int j = 0; j <= max_id; ++j) {
                if (i != j) {
                    neighbors.push_back({distance(componentList[i], componentList[j]), j});
                }
            }
            sort(neighbors.begin(), neighbors.end());
            int max_k = min(64, (int)neighbors.size());
            for (int k = 0; k < max_k; ++k) {
                nn_list[i][k] = neighbors[k].second;
            }
            for (int k = max_k; k < 64; ++k) {
                nn_list[i][k] = -1;
            }
        }
    }

    long double distance(const Component& i, const Component& j) const {
        if (dist_matrix.empty() || i.id >= dist_matrix.size() || j.id >= dist_matrix.size()) {
            return ::distance(i.id, j.id);
        }
        if (dist_matrix[i.id][j.id] < 0) {
            long double d = ::distance(i.id, j.id);
            dist_matrix[i.id][j.id] = d;
            dist_matrix[j.id][i.id] = d;
            return d;
        }
        return dist_matrix[i.id][j.id];
    }
    long double get_cost(const Component& i, const Component& j, long double vmax, long double vmin, long double w, long double W) const {
        auto d = distance(i, j);
        auto v = vmax - w * (vmax - vmin) / W;
        return d / v;
    } 
} connections;

struct State {
    const Component* sequence_arr[MAX_CITIES];
    int sequence_len = 0;
    double hypothetical_weight = 0.0;
    double solution_cost() {
        int tour_arr[MAX_CITIES];
        int tour_len = 0;
        for (int i = 0; i < sequence_len; i++) tour_arr[tour_len++] = sequence_arr[i]->id;
        Packing p1 = pack(tour_arr, tour_len);
        double f1 = computeFitness(tour_arr, tour_len, p1);
        
        int rev_tour_arr[MAX_CITIES];
        for (int i = 0; i < tour_len; i++) rev_tour_arr[i] = tour_arr[i];
        
        // Reverse everything except the first and last element (which are the start/end cities)
        for (int i = 1; i < (tour_len - 1) / 2 + 1; i++) {
            swap(rev_tour_arr[i], rev_tour_arr[tour_len - 1 - i]);
        }
        Packing p2 = pack(rev_tour_arr, tour_len);
        double f2 = computeFitness(rev_tour_arr, tour_len, p2);
        
        if (f2 > f1) {
            // Keep the reversed sequence if it's better
            for (int i = 1; i < (sequence_len - 1) / 2 + 1; i++) {
                swap(sequence_arr[i], sequence_arr[sequence_len - 1 - i]);
            }
            return f2;
        }
        return f1;
    }
    void restart() {
        sequence_len = 0;
    }
};

bool termination_criterion();

const State* global_best_state = nullptr;

#include "pheromone/pheromone.cpp"
#include "colony/ant.cpp"
#include "colony/colony.cpp"
#include "daemon_actions/optional.cpp"

bool termination_criterion() {
    return elapsed_time() >= params.time_limit * 0.95;
}

void ACOinit() {
    log_debug("aco: initializing ACO solver");
    const int n = static_cast<int>(nCities);

    seed = params.seed;
    rng.seed(seed);

    assert(n > 0);
    assert(params.number_of_ants > 0);
    assert(params.time_limit > 0.0L);

    // 1. Build components from THOP cities
    int max_id = 0;
    for (size_t i = 0; i < nCities; ++i) {
        componentList[i].id = nodes[i].id;
        componentList[i].x = nodes[i].x;
        componentList[i].y = nodes[i].y;
        componentList[i].totalWeight = nodes[i].totalWeight;
        componentList[i].totalProfit = nodes[i].totalProfit;
        max_id = max(max_id, componentList[i].id);
    }
    numComponents = nCities;
    connections.init(max_id);

    // 2. Initialize pheromone matrix
    Pheromone.init(numComponents);

    for (int i = 0; i < params.number_of_ants; i++) {
        Colony.ants[i].initialize(numComponents);
    }

    log_debug("aco: initialized ", numComponents, " components and ",
         params.number_of_ants, " ants");
}