#include <bits/stdc++.h>
using namespace std;

struct Component {
    int id;
    int x, y;
};
vector<Component> componentList;
vector<vector<int>> candidateList;
vector<vector<long double>> candidateEta;

struct Connections {
    long double distance(Component i, Component j) {
        return ceil(sqrt((i.x - j.x) * (i.x - j.x) + (i.y - j.y) * (i.y - j.y)));
    }
    long double distance_sq(Component i, Component j) {
        return (i.x - j.x) * (i.x - j.x) + (i.y - j.y) * (i.y - j.y);
    }
    long double get_cost(Component i, Component j, long double vmax, long double vmin, long double w, long double W) {
        auto d = distance(i, j);
        auto v = vmax - w * (vmax - vmin) / W;
        return d / v;
    } 
} connections;

struct State {
    vector<Component> sequence;
    long double cached_cost = -INF;
    bool cost_computed = false;
    Packing cached_packing;
    
    long double solution_cost() {
        if (cost_computed) return cached_cost;
        vector<int> tour;
        for (auto c : sequence) tour.push_back(c.id);
        cached_packing = pack(tour);
        cached_cost = computeFitness(tour, cached_packing);
        cost_computed = true;
        return cached_cost;
    }
    void restart() {
        sequence.resize(0);
        cost_computed = false;
        cached_packing.picked.clear();
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

    log_debug("aco: initializing candidate lists");
    const int K = min(50, n - 1);
    candidateList.assign(n, vector<int>());
    candidateEta.assign(n, vector<long double>());
    for (int i = 0; i < n; ++i) {
        vector<pair<long double, int>> dists;
        dists.reserve(n - 1);
        for (int j = 0; j < n; ++j) {
            if (i != j) dists.push_back({connections.distance_sq(componentList[i], componentList[j]), j});
        }
        partial_sort(dists.begin(), dists.begin() + K, dists.end());
        for (int k = 0; k < K; ++k) {
            candidateList[i].push_back(dists[k].second);
            candidateEta[i].push_back(pow(1.0L / sqrt(dists[k].first), params.beta));
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