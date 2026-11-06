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
        static vector<double> pre_ls_cost;
        static vector<int>    pre_seq;
        static vector<int>    pre_seq_size;
        static vector<uint8_t> pre_picked;
        pre_ls_cost.assign(Colony.antsCount, 0.0);
        pre_seq_size.assign(Colony.antsCount, 0);
        pre_seq.assign((size_t)Colony.antsCount * MAX_CITIES, 0);
        if (params.local_search_flag > 0)
            pre_picked.assign((size_t)Colony.antsCount * nItems, 0);
        fprintf(stderr, "[dbg] upd_stat it=%d phase1 start\n", iteration);
        for (int a = 0; a < Colony.antsCount; ++a) {
            Ant& ant = Colony.ants[a];
            if (ant.state.sequenceSize == 0) { pre_ls_cost[a] = -INF; pre_seq_size[a] = 0; continue; }
            auto t1 = chrono::high_resolution_clock::now();
            pre_ls_cost[a] = ant.state.solution_cost();
            auto t2 = chrono::high_resolution_clock::now();
            pack_time += chrono::duration_cast<chrono::microseconds>(t2 - t1).count();
            fprintf(stderr, "[dbg] upd_stat it=%d pack ant %d sz=%d done\n", iteration, a, ant.state.sequenceSize);
            pre_seq_size[a] = ant.state.sequenceSize;
            for (int i = 0; i < ant.state.sequenceSize; ++i) pre_seq[(size_t)a * MAX_CITIES + i] = ant.state.sequence[i];
            if (params.local_search_flag > 0)
                memcpy(&pre_picked[(size_t)a * nItems], ant.state.packing.picked, nItems);
        }

        // Phase 2 (matches realbench local_search): LS + repack + revert, per ant.
        // realbench calls termination_condition() after each ant's compute_fitness (pack)
        // inside local_search() — we replicate that here.
        for (int a = 0; a < Colony.antsCount; ++a) {
            Ant& ant = Colony.ants[a];
            if (ant.state.sequenceSize == 0) continue;
            double cost = pre_ls_cost[a];
            if (params.local_search_flag > 0) {
                auto t3 = chrono::high_resolution_clock::now();
                fprintf(stderr, "[dbg] upd_stat it=%d ls_run ant %d sz=%d\n", iteration, a, ant.state.sequenceSize);
                ls_run(ant.state, params.local_search_flag);
                auto t4 = chrono::high_resolution_clock::now();
                fprintf(stderr, "[dbg] upd_stat it=%d ls_run ant %d done\n", iteration, a);
                ls_time += chrono::duration_cast<chrono::microseconds>(t4 - t3).count();
                double post_ls_cost = ant.state.solution_cost();
                auto t5 = chrono::high_resolution_clock::now();
                pack_time += chrono::duration_cast<chrono::microseconds>(t5 - t4).count();
                if (post_ls_cost < pre_ls_cost[a]) {
                    ant.state.sequenceSize = pre_seq_size[a];
                    for (int i = 0; i < pre_seq_size[a]; ++i) ant.state.sequence[i] = pre_seq[(size_t)a * MAX_CITIES + i];
                    memcpy(ant.state.packing.picked, &pre_picked[(size_t)a * nItems], nItems);
                } else {
                    cost = post_ls_cost;
                }
                // === matches realbench local_search(): if (termination_condition()) return; ===
                if (cost > iter_best_cost) { iter_best_cost = cost; iter_best_ant = &ant; }
                if (termination_criterion()) goto phase2_done;
                continue;   // skip the duplicate iter_best update below when LS is on
            }
            if (cost > iter_best_cost) { iter_best_cost = cost; iter_best_ant = &ant; }
        }
        phase2_done:
        // cout << "Pack time: " << pack_time / 1000.0 << "ms, LS time: " << ls_time / 1000.0 << "ms" << endl;

        if (iter_best_ant == nullptr) return;

        // best-so-far
        bool is_new_best = (!has_best_solution || iter_best_cost > best_cost);
        if (is_new_best) {
            best_cost = iter_best_cost;
            best_solution = iter_best_ant->state;
            has_best_solution = true;
            restart_found_best = iteration;
            log_debug("aco: new best solution found with cost ", (long int)best_cost,
                 " and ", best_solution.sequenceSize, " components");
        } else {
            log_debug("aco: generation ", iteration,
                 " iter_best with cost ", (long int)iter_best_cost,
                 " and ", iter_best_ant->state.sequenceSize,
                 " components, best_so_far = ", (long int)best_cost);
        }
        printf("best %10ld,        iteration: %10ld,        time %10.2f\n",
               (long int)best_cost, (long int)iteration, elapsed_time());
        fflush(stdout);
        if (log_report_file) {
            fprintf(log_report_file, "best %10ld,        iteration: %10ld,        time %10.2f\n",
                    (long int)best_cost, (long int)iteration, elapsed_time());
            fflush(log_report_file);
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
            else if (has_restart_best)
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
            if (params.esaco3) {
                long t2 = esaco3_tier2, t3 = esaco3_tier3;
                double hit = (t2 + t3 > 0) ? (double)t2 / (double)(t2 + t3) : 0.0;
                log_debug("aco: ESACO S3 tier2=", t2, " tier3(fullscan)=", t3, " hit=", hit);
            }
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
        // realbench only LS on complete 0->destination open tours. Skip partial
        // ones (e.g. ants cut off mid-construction) so two_opt_first never sees
        // an invalid cycle.
        if (s.sequence[s.sequenceSize - 1] != nCities - 1) return;
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

// ESACO S1 helper: fill succ[city] = successor of city in the best-so-far
// open tour (or -1). O(n) per LS call instead of O(n) per node (bug 1 fix).
void fill_best_tour_succ(long int* succ, long int size) {
    for (long int i = 0; i < size; ++i) succ[i] = -1;
    if (!Optional.has_best_solution) return;
    int sz = Optional.best_solution.sequenceSize;
    for (int i = 0; i + 1 < sz; ++i)
        succ[Optional.best_solution.sequence[i]] = Optional.best_solution.sequence[i + 1];
}

