#include <bits/stdc++.h>
using namespace std;

struct Component {
    int id;
    int idCity;
    int idItem;
};
vector<Component> componentList;

struct Connections {
    long double distance(Component i, Component j) {
        return ::distance(nodes[i.idCity], nodes[j.idCity]);
    }
    long double get_cost(Component i, Component j, long double vmax, long double vmin, long double w, long double W) {
        auto d = distance(i, j);
        auto v = vmax - w * (vmax - vmin) / W;
        return d / v;
    } 
} connections;

struct State {
    vector<Component> sequence;
    long long current_weight = 0;
    long double current_time = 0.0L;
    long long current_profit = 0;
    int last_city = 0;

    void add_component(const Component& c) {
        current_time += ::distance(last_city, c.idCity) / (maxSpeed - current_weight * (maxSpeed - minSpeed) / capacity);
        current_weight += items[c.idItem].weight;
        current_profit += items[c.idItem].profit;
        last_city = c.idCity;
        sequence.push_back(c);
    }

    bool can_add(const Component& c) const {
        long long next_weight = current_weight + items[c.idItem].weight;
        if (next_weight > capacity) return false;
        long double next_time = current_time + ::distance(last_city, c.idCity) / (maxSpeed - current_weight * (maxSpeed - minSpeed) / capacity);
        long double time_to_end = ::distance(c.idCity, nCities - 1) / (maxSpeed - next_weight * (maxSpeed - minSpeed) / capacity);
        if (next_time + time_to_end > maxTime) return false;
        return true;
    }

    long double solution_cost() const {
        if (current_weight > capacity) return -INF;
        if (current_time + ::distance(last_city, nCities - 1) / (maxSpeed - current_weight * (maxSpeed - minSpeed) / capacity) > maxTime) return -INF;
        return current_profit;
    }

    void recalculate() {
        current_weight = 0;
        current_time = 0.0L;
        current_profit = 0;
        last_city = 0;
        for (const auto& c : sequence) {
            current_time += ::distance(last_city, c.idCity) / (maxSpeed - current_weight * (maxSpeed - minSpeed) / capacity);
            current_weight += items[c.idItem].weight;
            current_profit += items[c.idItem].profit;
            last_city = c.idCity;
        }
    }

    void restart() {
        sequence.resize(0);
        current_weight = 0;
        current_time = 0.0L;
        current_profit = 0;
        last_city = 0;
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
    const int n = static_cast<int>(nItems);

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
        c.idCity = items[i].city;
        c.idItem = items[i].id;
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