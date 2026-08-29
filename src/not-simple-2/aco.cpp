#include <bits/stdc++.h>
using namespace std;

struct Component {
    int id;
    int x, y;
};
Component componentList[MAX_CITIES];
int knn[MAX_CITIES][MAX_KN];
int NN_ANTS = 0;

struct Connections {
    double distance(Component i, Component j) {
        return ::distance(i.id, j.id);
    }
    double get_cost(Component i, Component j, double vmax, double vmin, double w, double W) {
        auto d = distance(i, j);
        auto v = vmax - w * (vmax - vmin) / W;
        return d / v;
    }
} connections;

struct State {
    int sequence[MAX_CITIES];   // component ids (holds only city ids)
    int sequenceSize = 0;
    Packing packing;

    double solution_cost() {
        packing = pack(sequence, sequenceSize);
        return packing.totalProfit;
    }
    void restart() {
        sequenceSize = 0;
    }
};

bool termination_criterion();

#include "pheromone/pheromone.cpp"
#include "colony/ant.cpp"
#include "colony/colony.cpp"
#include "ls/ls.cpp"
#include "daemon_actions/optional.cpp"

bool termination_criterion() {
    return elapsed_time() >= params.time_limit;
}

// Port of aco++ nn_tour(): greedy nearest-neighbor tour + LS, then seed the
// best-so-far with it (copy_from_to(&ant[0], best_so_far_ant)) and return its
// FITNESS for trail_0 = 1/(rho * fitness).
double init_from_nn_tour() {
    int tour[MAX_CITIES];
    bool visited[MAX_CITIES];
    memset(visited, 0, nCities);
    int cur = 0;
    visited[cur] = true;
    int len = 0;
    tour[len++] = 0;
    for (int step = 0; step < nCities - 2; ++step) {
        int next = -1;
        long int best = numeric_limits<long int>::max();
        for (int j = 0; j < nCities; ++j) {
            if (visited[j] || j == nCities - 1) continue;   // destination last
            long int d = (long int)(distance(cur, j) + 0.5);
            if (d < best) { best = d; next = j; }
        }
        tour[len++] = next;
        visited[next] = true;
        cur = next;
    }
    tour[len++] = nCities - 1;
    // Open tour (0..nCities-1); ls_run()/pack() add the dummy+wrap like the main loop.

    // apply LS (aco++ nn_tour applies two_opt/two_h_opt/three_opt per ls_flag)
    State s;
    for (int i = 0; i < len; ++i) s.sequence[i] = tour[i];
    s.sequenceSize = len;
    if (params.local_search_flag > 0) {
        Optional.ls_run(s, params.local_search_flag);
    }

    Packing p = pack(s.sequence, s.sequenceSize);
    s.packing = p;

    // seed the best-so-far with the NN tour (like aco++ nn_tour)
    Optional.best_solution = s;
    Optional.best_cost = p.totalProfit;
    Optional.has_best_solution = true;

    double UB = compute_upper_bound();
    return UB + 1.0 - p.totalProfit;   // fitness
}

void ACOinit() {
    log_debug("aco: initializing ACO solver");
    const int n = static_cast<int>(nCities) + 1; // +1 for the uninitialized dummy node matching realbench

    ls_init();
    seed = params.seed;

    assert(n > 0);
    assert(params.number_of_ants > 0);
    assert(params.time_limit > 0.0);
    assert(n <= MAX_CITIES);
    assert(params.number_of_ants <= MAX_ANTS);
    assert(nItems <= MAX_ITEMS);

    // MMAS: upper bound = sum of all item profits (fitness = UB + 1 - profit)
    Optional.init_upper_bound();

    // 1. Build components from THOP cities
    for (int i = 0; i < nCities; ++i) {
        componentList[i].id = i;
        componentList[i].x = nodes[i].x;
        componentList[i].y = nodes[i].y;
    }
    // Add dummy node with (0,0)
    componentList[nCities].id = nCities;
    componentList[nCities].x = 0;
    componentList[nCities].y = 0;

    // Initialize k-nearest neighbors for local search
    NN_ANTS = min(20, n - 1);
    auto swap2 = [](long int* v, long int* v2, long int i, long int j) {
        long int tmp = v[i]; v[i] = v[j]; v[j] = tmp;
        tmp = v2[i]; v2[i] = v2[j]; v2[j] = tmp;
    };
    auto sort2 = [&swap2](auto& self, long int* v, long int* v2, long int left, long int right) -> void {
        long int k, last;
        if (left >= right) return;
        swap2(v, v2, left, (left + right)/2);
        last = left;
        for (k=left+1; k <= right; k++) {
            if (v[k] < v[left]) swap2(v, v2, ++last, k);
        }
        swap2(v, v2, left, last);
        self(self, v, v2, left, last);
        self(self, v, v2, last+1, right);
    };

    static long int distance_vector[MAX_CITIES];
    static long int help_vector[MAX_CITIES];
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            double d = connections.distance(componentList[i], componentList[j]);
            distance_vector[j] = (long int)(d + 0.5);
            help_vector[j] = j;
        }
        distance_vector[i] = numeric_limits<long int>::max(); // LONG_MAX equivalent
        sort2(sort2, distance_vector, help_vector, 0, n - 1);
        for (int j = 0; j < NN_ANTS; ++j) knn[i][j] = help_vector[j];
    }

    // 2. Initialize pheromone matrix + best-so-far from the NN tour (like aco++)
    double trail_0 = 1.0 / (params.rho * init_from_nn_tour());
    Pheromone.init(n, trail_0);

    // 3. Create colony
    Colony.antsCount = params.number_of_ants;

    for (int a = 0; a < Colony.antsCount; ++a) {
        Colony.ants[a].initialize(n);
    }

    log_debug("aco: initialized ", n, " components and ",
         Colony.antsCount, " ants");
}
