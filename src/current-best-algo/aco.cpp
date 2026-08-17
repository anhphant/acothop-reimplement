#include <bits/stdc++.h>
using namespace std;

struct Component {
    int id;
    int x, y;
    double totalWeight = 0.0; 
    double totalProfit = 0.0;
};
vector<Component> componentList;

struct Connections {
    vector<vector<long double>> dist_matrix;
    
    void init(int max_id) {
        dist_matrix.assign(max_id + 1, vector<long double>(max_id + 1, -1.0));
    }

    long double distance(Component i, Component j) {
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
    long double get_cost(Component i, Component j, long double vmax, long double vmin, long double w, long double W) {
        auto d = distance(i, j);
        auto v = vmax - w * (vmax - vmin) / W;
        return d / v;
    } 
} connections;

struct State {
    vector<Component> sequence;
    double hypothetical_weight = 0.0;
    long double solution_cost() {
        vector<int> tour;
        for (auto c : sequence) tour.push_back(c.id);
        Packing p1 = pack(tour);
        double f1 = computeFitness(tour, p1);
        
        vector<int> rev_tour = tour;
        reverse(rev_tour.begin() + 1, rev_tour.end() - 1);
        Packing p2 = pack(rev_tour);
        double f2 = computeFitness(rev_tour, p2);
        
        if (f2 > f1) {
            // Keep the reversed sequence if it's better
            reverse(sequence.begin() + 1, sequence.end() - 1);
            return f2;
        }
        return f1;
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
    componentList.clear();
    componentList.reserve(n);

    int max_id = 0;
    for (int i = 0; i < n; ++i) {
        Component c;
        c.id = i;
        c.x = nodes[i].x;
        c.y = nodes[i].y;
        c.totalWeight = nodes[i].totalWeight;
        c.totalProfit = nodes[i].totalProfit;
        componentList.push_back(c);
        max_id = max(max_id, c.id);
    }
    connections.init(max_id);

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