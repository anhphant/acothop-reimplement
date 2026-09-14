#include <bits/stdc++.h>
using namespace std;

struct Component {
    int id;
    int x, y;
};
Component componentList[MAX_CITIES];
int knn[MAX_CITIES][MAX_KN];
int knn_backup[MAX_CITIES][MAX_KN_BACKUP];
int knn_back_pos[MAX_CITIES][MAX_KN];
int NN_ANTS = 0;
int NN_BACKUP_ANTS = 0;

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
        if (next == -1) { fprintf(stderr, "CRASH next is -1 at step %d\n", step); exit(1); }
        if (step % 100 == 0) { fprintf(stderr, "step %d\n", step); fflush(stderr); }
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

    // Initialize k-nearest neighbors using combined score: (1/d)^beta1 * profit^beta2
    // knn[i]        = top MAX_KN       by score (rank 1-20)
    // knn_backup[i] = next MAX_KN_BACKUP by score (rank 21-40)
    NN_ANTS = min(MAX_KN, n - 1);
    NN_BACKUP_ANTS = min(MAX_KN_BACKUP, max(0, n - 1 - NN_ANTS));

    double b1 = params.beta1;
    double b2 = params.beta2;

    static double score_vector[MAX_CITIES];
    static int    idx_vector[MAX_CITIES];

    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            idx_vector[j] = j;
            if (j == i) { score_vector[j] = -1.0; continue; }
            double d      = connections.distance(componentList[i], componentList[j]);
            double dist_f = (b1 != 0.0) ? pow(1.0 / (d + params.eps1), b1) : 1.0;
            double profit = max(city_profit[j], 1.0);
            double prof_f = (b2 != 0.0) ? pow(profit, b2) : 1.0;
            score_vector[j] = dist_f * prof_f;
        }
        // partial_sort: chỉ sort top NN_ANTS+NN_BACKUP_ANTS phần tử, descending
        int top = min(NN_ANTS + NN_BACKUP_ANTS + 1, n);  // +1 cho phòng self
        partial_sort(idx_vector, idx_vector + top, idx_vector + n,
                     [](int a, int b_) { return score_vector[a] > score_vector[b_]; });

        // Điền knn (skip self)
        int cnt = 0, pos = 0;
        while (cnt < NN_ANTS && pos < n) {
            if (idx_vector[pos] != i) knn[i][cnt++] = idx_vector[pos];
            pos++;
        }

        // Điền knn_backup (tiếp tục từ pos, skip self)
        cnt = 0;
        while (cnt < NN_BACKUP_ANTS && pos < n) {
            if (idx_vector[pos] != i) knn_backup[i][cnt++] = idx_vector[pos];
            pos++;
        }
    }
    fprintf(stderr, "knn built\n");
      log_debug("aco: knn + knn_backup built (b1=", b1, ", b2=", b2,
              ", NN_ANTS=", NN_ANTS, ", NN_BACKUP=", NN_BACKUP_ANTS, ")");

    for (int i = 0; i < n; ++i) {
        for (int k = 0; k < NN_ANTS; ++k) {
            int j = knn[i][k];
            knn_back_pos[i][k] = -1;
            for (int m = 0; m < NN_ANTS; ++m) {
                if (knn[j][m] == i) {
                    knn_back_pos[i][k] = m;
                    break;
                }
            }
        }
    }

    // 2. Initialize pheromone matrix + best-so-far from the NN tour (like aco++)
          double nn_fitness = init_from_nn_tour();
      double trail_0 = 1.0 / (params.rho * nn_fitness);
      fprintf(stderr, "After init_from_nn_tour\n");
    Pheromone.init(n, trail_0);

    // 3. Create colony
    Colony.antsCount = params.number_of_ants;

    for (int a = 0; a < Colony.antsCount; ++a) {
        Colony.ants[a].initialize(n);
    }

    log_debug("aco: initialized ", n, " components and ",
         Colony.antsCount, " ants");
}
