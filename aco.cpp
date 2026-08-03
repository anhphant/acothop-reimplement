#include <bits/stdc++.h>
using namespace std;

struct Component {
    int id;
    int x, y;
};
vector<Component> componentList;

struct Connections {
    long double distance(Component i, Component j) {
        return sqrt((i.x - j.x) * (i.x - j.x) + (i.y - j.y) * (i.y - j.y));
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