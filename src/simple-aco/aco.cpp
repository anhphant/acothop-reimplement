#include <bits/stdc++.h>
using namespace std;

struct Component {
    int id;
    int x, y;
};
vector<Component> componentList;
vector<vector<int>> knn;

struct Connections {
    long double distance(Component i, Component j) {
        return ::distance(i.id, j.id);
    }
    long double get_cost(Component i, Component j, long double vmax, long double vmin, long double w, long double W) {
        auto d = distance(i, j);
        auto v = vmax - w * (vmax - vmin) / W;
        return d / v;
    } 
} connections;

struct State {
    vector<Component> sequence;
    long double solution_cost() {
        vector<int> tour;
        for (auto c : sequence) tour.push_back(c.id);
        Packing p = pack(tour);
        return computeFitness(tour, p);
    }
    void restart() {
        sequence.resize(0);
    }
};

bool termination_criterion();

#include "pheromone/pheromone.cpp"
#include "colony/ant.cpp"
#include "colony/colony.cpp"
#include "daemon_actions/optional.cpp"

bool termination_criterion() {
    return elapsed_time() >= params.time_limit;
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
    componentList.clear();
    componentList.reserve(n);

    for (int i = 0; i < n; ++i) {
        Component c;
        c.id = i;
        c.x = nodes[i].x;
        c.y = nodes[i].y;
        componentList.push_back(c);
    }

    // Initialize k-nearest neighbors for local search
    const int NN_ANTS = min(20, n - 1);
    knn.assign(n, vector<int>(NN_ANTS));
    for (int i = 0; i < n; ++i) {
        vector<pair<long double, int>> neighbors;
        for (int j = 0; j < n; ++j) {
            if (i == j) continue;
            neighbors.push_back({connections.distance(componentList[i], componentList[j]), j});
        }
        sort(neighbors.begin(), neighbors.end());
        for (int k = 0; k < NN_ANTS; ++k) {
            knn[i][k] = neighbors[k].second;
        }
    }

    // 2. Initialize pheromone matrix
    Pheromone.init(componentList.size());

    // 3. Create colony
    Colony.ants.clear();
    Colony.ants.resize(params.number_of_ants);

    for (auto& ant : Colony.ants) {
        ant.initialize(componentList.size());
    }

    log_debug("aco: initialized ", componentList.size(), " components and ",
         Colony.ants.size(), " ants");
}