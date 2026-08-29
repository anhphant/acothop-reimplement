class Optional {
public:
    State best_solution;                  // best-so-far (global best)
    double best_cost = -INF;         // profit of best_solution (maximize)
    bool has_best_solution = false;

    State restart_best_solution;          // best since last restart
    double restart_best_cost = -INF;
    bool has_restart_best = false;

    double UB = 0;                   // sum of all item profits (for fitness)

    int u_gb = 25;                        // update global/restart best every u_gb iterations
    int restart_iteration = 1;            // iteration when last restart happened (realbench init_try)
    int restart_found_best = 0;           // iteration when restart-best was found/improved
    double lambda = 0.05;                 // lambda-branching factor parameter
    double branch_fac = 1.00001;          // stagnation threshold
    double branching_factor = 0.0;

    Ant* iter_best_ant = nullptr;         // iteration-best ant
    double iter_best_cost = -INF;

    // THOP is maximization; convert profit to a minimization fitness
    double fitness(double cost) const { return UB + 1.0 - cost; }

    void init_upper_bound() {
        UB = compute_upper_bound();
    }

    void deposit_solution(State& s, double cost) {
        if (s.sequenceSize < 2) return;
        double d_tau = 1.0 / fitness(cost);
        for (int i = 0; i + 1 < s.sequenceSize; ++i)
            Pheromone.deposit_pheromone_on_the_visited_arc(
                s.sequence[i], s.sequence[i + 1], d_tau);
        // realbench global_update_pheromone deposits on the CLOSED tour, which
        // includes the dummy node arcs: dest -> dummy and dummy -> start.
        const int dummy = nCities;
        Pheromone.deposit_pheromone_on_the_visited_arc(
            s.sequence[s.sequenceSize - 1], dummy, d_tau);
        Pheromone.deposit_pheromone_on_the_visited_arc(
            dummy, s.sequence[0], d_tau);
    }

    /*------------------------------------------------------------*/

    // Evaluate every ant with the knapsack, apply LS with revert, track iteration-best.
        void update_statistics(int iteration) {
        double pack_time = 0, ls_time = 0;
        iter_best_cost = -INF; iter_best_ant = nullptr;

        // Phase 1 (matches realbench construct_solutions): pack ALL ants first,
        // so the RNG stream matches realbench (which packs every ant before LS).
        static double pre_ls_cost[MAX_ANTS];
        static int    pre_seq[MAX_ANTS][MAX_CITIES];
        static int    pre_seq_size[MAX_ANTS];
        for (int a = 0; a < Colony.antsCount; ++a) {
            Ant& ant = Colony.ants[a];
            if (ant.state.sequenceSize == 0) { pre_ls_cost[a] = -INF; pre_seq_size[a] = 0; continue; }
            auto t1 = chrono::high_resolution_clock::now();
            pre_ls_cost[a] = ant.state.solution_cost();
            auto t2 = chrono::high_resolution_clock::now();
            pack_time += chrono::duration_cast<chrono::microseconds>(t2 - t1).count();
            pre_seq_size[a] = ant.state.sequenceSize;
            for (int i = 0; i < ant.state.sequenceSize; ++i) pre_seq[a][i] = ant.state.sequence[i];
        }

        // Phase 2 (matches realbench local_search): LS + repack + revert, per ant.
        for (int a = 0; a < Colony.antsCount; ++a) {
            Ant& ant = Colony.ants[a];
            if (ant.state.sequenceSize == 0) continue;
            double cost = pre_ls_cost[a];
            if (params.local_search_flag > 0) {
                auto t3 = chrono::high_resolution_clock::now();
                ls_run(ant.state, params.local_search_flag);
                auto t4 = chrono::high_resolution_clock::now();
                ls_time += chrono::duration_cast<chrono::microseconds>(t4 - t3).count();
                double post_ls_cost = ant.state.solution_cost();
                auto t5 = chrono::high_resolution_clock::now();
                pack_time += chrono::duration_cast<chrono::microseconds>(t5 - t4).count();
                if (post_ls_cost < pre_ls_cost[a]) {
                    ant.state.sequenceSize = pre_seq_size[a];
                    for (int i = 0; i < pre_seq_size[a]; ++i) ant.state.sequence[i] = pre_seq[a][i];
                } else {
                    cost = post_ls_cost;
                }
            }
            if (cost > iter_best_cost) { iter_best_cost = cost; iter_best_ant = &ant; }
        }
        cout << "Pack time: " << pack_time / 1000.0 << "ms, LS time: " << ls_time / 1000.0 << "ms" << endl;

        if (iter_best_ant == nullptr) return;

        // best-so-far
        if (!has_best_solution || iter_best_cost > best_cost) {
            best_cost = iter_best_cost;
            best_solution = iter_best_ant->state;
            has_best_solution = true;
            restart_found_best = iteration;
            log_debug("aco: new best solution found with cost ", iter_best_cost,
                 " and ", best_solution.sequenceSize, " components");
        }

        // restart-best
        if (!has_restart_best || iter_best_cost > restart_best_cost) {
            restart_best_cost = iter_best_cost;
            restart_best_solution = iter_best_ant->state;
            has_restart_best = true;
            restart_found_best = iteration;
        }

        // Update MMAS trail limits from best-so-far fitness (realbench update_statistics).
        if (has_best_solution)
            Pheromone.update_trail_limits(fitness(best_cost));
    }

    /*------------------------------------------------------------*/

    double node_branching(double l) {
        int n = nCities;
        double total = 0.0;
        for (int m = 0; m < n; ++m) {
            double min = (double)Pheromone.getPheromone(m, knn[m][0]);
            double max = min;
            for (int k = 0; k < NN_ANTS; ++k) {
                double p = (double)Pheromone.getPheromone(m, knn[m][k]);
                if (p > max) max = p;
                if (p < min) min = p;
            }
            double cutoff = min + l * (max - min);
            double count = 0.0;
            for (int k = 0; k < NN_ANTS; ++k) {
                if ((double)Pheromone.getPheromone(m, knn[m][k]) > cutoff)
                    count += 1.0;
            }
            total += count;
        }
        // normalize branching factor to minimal value 1
        return total / (double)(n * 2);
    }

    /*------------------------------------------------------------*/

    void daemon_actions(int iteration) {
        // MMAS global pheromone update: iteration-best or best-so-far/restart-best
        if (iteration % u_gb) {
            if (iter_best_ant != nullptr)
                deposit_solution(iter_best_ant->state, iter_best_cost);
        } else {
            if (u_gb == 1 && (iteration - restart_found_best > 50))
                deposit_solution(best_solution, best_cost);
            else
                deposit_solution(restart_best_solution, restart_best_cost);
        }

        // u_gb schedule (only applied when local search is used)
        if (params.local_search_flag > 0) {
            if ((iteration - restart_iteration) < 25)
                u_gb = 25;
            else if ((iteration - restart_iteration) < 75)
                u_gb = 5;
            else if ((iteration - restart_iteration) < 125)
                u_gb = 3;
            else if ((iteration - restart_iteration) < 250)
                u_gb = 2;
            else
                u_gb = 1;
        } else {
            u_gb = 25;
        }

        // realbench: check_pheromone_trail_limits is only called for MMAS WITHOUT local
        // search. With LS, trail_min is enforced inside mmas_evaporation_nn_list.
        if (params.local_search_flag == 0 && has_best_solution)
            Pheromone.check_pheromone_trail_limits(fitness(best_cost));
    }

    /*------------------------------------------------------------*/

    void search_control_and_statistics(int iteration) {
        if (!(iteration % 100)) {
            branching_factor = node_branching(lambda);
            if (branching_factor < branch_fac && (iteration - restart_found_best > 250)) {
                Pheromone.reset_pheromone();
                restart_iteration = iteration;
                restart_best_cost = -INF;
                has_restart_best = false;
                log_debug("aco: stagnation (branching_factor=", branching_factor,
                     "); pheromone re-initialized at iteration ", iteration);
            }
        }
    }

    /*------------------------------------------------------------*/

    // Faithful adapter: run the ported realbench local search on a State tour.
    // Builds a closed tour [sequence..., dummy, wrap], runs the ported LS, then
    // extracts back the open path (walk from 0 until hitting the dummy node).
    void ls_run(State& s, int ls_flag) {
        if (s.sequenceSize < 2) return;
        const int dummy = nCities;
        static long int tour[MAX_CITIES + 2];
        const int t_size = s.sequenceSize + 2;   // + dummy + wrap
        for (int i = 0; i < s.sequenceSize; ++i) tour[i] = s.sequence[i];
        tour[s.sequenceSize] = dummy;
        tour[s.sequenceSize + 1] = tour[0];

        switch (ls_flag) {
            case 1: two_opt_first(tour, t_size); break;
            case 2: two_h_opt_first(tour, t_size); break;
            case 3: three_opt_first(tour, t_size); break;
            default: return;
        }

        const int n = t_size - 1;
        s.sequenceSize = 0;
        for (int i = 0; i < n; ++i) {
            if (tour[i] == dummy) break;
            s.sequence[s.sequenceSize++] = (int)tour[i];
        }
    }

    void local_search() {
        if (!has_best_solution) {
            return;
        }
        ls_run(best_solution, params.local_search_flag);
    }

    void reset() {
        best_solution.restart();
        best_cost = -INF;
        has_best_solution = false;
        restart_best_solution.restart();
        restart_best_cost = -INF;
        has_restart_best = false;
        u_gb = 25;
        restart_iteration = 1;
        restart_found_best = 0;
    }
} Optional;

