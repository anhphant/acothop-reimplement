class Optional {
public:
    State best_solution;
    long double best_cost = -INF;
    bool has_best_solution = false;

    void save_best_solution() {
        Ant* iter_best_ant = nullptr;
        long double iter_best_cost = -INF;
        
        pair<long double, Ant*> candidates[1005];
        int num_candidates = 0;

        for (int i = 0; i < params.number_of_ants; i++) {
            Ant& ant = Colony.ants[i];
            if (ant.state.sequence_len == 0 || ant.state.sequence_arr[ant.state.sequence_len-1]->id != componentList[numComponents - 1].id)
                continue;

            // Ultra-fast 2-opt sweep (3 passes) on all ants
            if (params.local_search_flag > 0) {
                two_opt(ant.state, 3, ant.perturbed_nodes);
            }
                
            // Compute pure geometric TSP distance in O(N)
            long double tsp_dist = 0.0L;
            for (size_t k = 0; k + 1 < ant.state.sequence_len; ++k) {
                tsp_dist += connections.distance(*ant.state.sequence_arr[k], *ant.state.sequence_arr[k+1]);
            }
            candidates[num_candidates++] = {tsp_dist, &ant};
        }

        if (num_candidates == 0) return;

        // Sort candidates
        stable_sort(candidates, candidates + num_candidates, [](const pair<long double, Ant*>& a, const pair<long double, Ant*>& b) {
            return a.first < b.first;
        });

        // Deep polish the best candidate
        if (params.local_search_flag > 0) {
            switch (params.local_search_flag) {
                case 1: two_opt(candidates[0].second->state, 30, candidates[0].second->perturbed_nodes); break;
                case 2: two_point_five_opt(candidates[0].second->state, 30, candidates[0].second->perturbed_nodes); break;
                case 3: three_opt(candidates[0].second->state, 30); break;
            }
        }

        // Only evaluate expensive Knapsack Packing on the Top K candidates (K = 5)
        size_t top_k = min(5, num_candidates);
        for (size_t i = 0; i < top_k; ++i) {
            Ant* ant = candidates[i].second;
            long double cost = ant->state.solution_cost();
            if (cost > iter_best_cost) {
                iter_best_cost = cost;
                iter_best_ant = ant;
            }
        }
        
        if (iter_best_ant != nullptr) {
            long double cost = iter_best_cost;

            if (!has_best_solution || cost > best_cost) {
                best_cost = cost;
                best_solution = iter_best_ant->state;
                has_best_solution = true;
                global_best_state = &best_solution;
                log_debug("aco: new best solution found with cost ", cost,
                     " and ", best_solution.sequence_len, " components");
            }
        }
    }

    void two_opt(State& s, int max_passes = 20, bool* perturbed_nodes = nullptr) {
        bool dont_look[MAX_CITIES];
        int pos[MAX_CITIES];
        for (int i = 0; i < s.sequence_len; i++) pos[s.sequence_arr[i]->id] = i;
        
        if (perturbed_nodes) {
            for (int i = 0; i < numComponents; i++) dont_look[i] = !perturbed_nodes[i];
        } else {
            for (int i = 0; i < numComponents; i++) dont_look[i] = false;
        }

        bool improved = true;
        int pass = 0;
        while (improved && ++pass <= max_passes) {
            improved = false;
            for (size_t i = 1; i + 2 < s.sequence_len; ++i) {
                int u_id = s.sequence_arr[i-1]->id;
                int v_id = s.sequence_arr[i]->id;
                if (dont_look[u_id] && dont_look[v_id]) continue;
                
                for (int k = 0; k < connections.nn_list_len; ++k) {
                    int x_id = connections.nn_list[v_id][k];
                    if (x_id == -1) break;
                    
                    int j = pos[x_id];
                    if (j <= i || j + 1 >= s.sequence_len) continue;
                    
                    int y_id = s.sequence_arr[j+1]->id;
                    
                    long double current_dist = connections.distance(componentList[u_id], componentList[v_id]) + 
                                               connections.distance(componentList[x_id], componentList[y_id]);
                    
                    long double new_dist = connections.distance(componentList[u_id], componentList[x_id]) + 
                                           connections.distance(componentList[v_id], componentList[y_id]);

                    if (new_dist < current_dist - 1e-6) {
                        for (int k2 = 0; k2 < (j - i + 1) / 2; k2++) {
                            swap(s.sequence_arr[i + k2], s.sequence_arr[j - k2]);
                        }
                        if (perturbed_nodes) {
                            for (int k2 = i; k2 <= j; k2++) perturbed_nodes[s.sequence_arr[k2]->id] = true;
                        }
                        for (int k2 = i; k2 <= j; k2++) pos[s.sequence_arr[k2]->id] = k2;
                        
                        dont_look[u_id] = dont_look[v_id] = false;
                        dont_look[x_id] = dont_look[y_id] = false;
                        improved = true;
                        break;
                    }
                }
            }
        }
    }

    void two_point_five_opt(State& s, int max_passes = 15, bool* perturbed_nodes = nullptr) {
        two_opt(s, max_passes, perturbed_nodes);

        bool dont_look[MAX_CITIES];
        int pos[MAX_CITIES];
        for (int i = 0; i < s.sequence_len; i++) pos[s.sequence_arr[i]->id] = i;
        if (perturbed_nodes) {
            for (int i = 0; i < numComponents; i++) dont_look[i] = !perturbed_nodes[i];
        } else {
            for (int i = 0; i < numComponents; i++) dont_look[i] = false;
        }

        bool improved = true;
        int pass = 0;
        while (improved && ++pass <= max_passes) {
            improved = false;

            // Node relocation (0.5 opt) sweep
            for (size_t i = 1; i + 1 < s.sequence_len; ++i) {
                int v_id = s.sequence_arr[i]->id;
                if (dont_look[v_id]) continue;
                
                int u_id = s.sequence_arr[i-1]->id;
                int w_id = s.sequence_arr[i+1]->id;

                for (int k = 0; k < connections.nn_list_len; ++k) {
                    int cand_id = connections.nn_list[v_id][k];
                    if (cand_id == -1) break;
                    
                    int cand_pos = pos[cand_id];
                    for (int j : {cand_pos, cand_pos + 1}) {
                        if (j <= 0 || j >= s.sequence_len) continue;
                        if (i == j || i == j + 1 || i + 1 == j) continue;

                        int x_id = s.sequence_arr[j-1]->id;
                        int y_id = s.sequence_arr[j]->id;
                        
                        long double current_dist = connections.distance(componentList[u_id], componentList[v_id]) + 
                                                   connections.distance(componentList[v_id], componentList[w_id]) + 
                                                   connections.distance(componentList[x_id], componentList[y_id]);
                        
                        long double new_dist = connections.distance(componentList[u_id], componentList[w_id]) + 
                                               connections.distance(componentList[x_id], componentList[v_id]) + 
                                               connections.distance(componentList[v_id], componentList[y_id]);

                        if (new_dist < current_dist - 1e-6) {
                            const Component* temp = s.sequence_arr[i];
                            if (i < j) {
                                for (size_t k2 = i; k2 < j - 1; ++k2) s.sequence_arr[k2] = s.sequence_arr[k2+1];
                                s.sequence_arr[j - 1] = temp;
                            } else {
                                for (size_t k2 = i; k2 > j; --k2) s.sequence_arr[k2] = s.sequence_arr[k2-1];
                                s.sequence_arr[j] = temp;
                            }
                            
                            int min_idx = min((int)i, (int)j - (i < j ? 1 : 0));
                            int max_idx = max((int)i, (int)j + (i > j ? 1 : 0));
                            for (int k2 = min_idx; k2 <= max_idx; k2++) pos[s.sequence_arr[k2]->id] = k2;
                            
                            if (perturbed_nodes) {
                                for (int k2 = min_idx; k2 <= max_idx; k2++) perturbed_nodes[s.sequence_arr[k2]->id] = true;
                            }
                            
                            dont_look[u_id] = dont_look[v_id] = dont_look[w_id] = false;
                            dont_look[x_id] = dont_look[y_id] = false;
                            improved = true;
                            break;
                        }
                    }
                    if (improved) break;
                }
            }
        }
    }

    void three_opt(State& s, int max_passes = 15) {
        two_point_five_opt(s, max_passes);
    }

    void local_search() {
        if (!has_best_solution) {
            return;
        }

        switch (params.local_search_flag) {
            case 0:
                // No local search
                break;

            case 1:
                // TODO: 2-opt
                two_opt(best_solution);
                break;

            case 2:
                // TODO: 2.5-opt
                two_point_five_opt(best_solution);
                break;

            case 3:
                // TODO: 3-opt
                three_opt(best_solution);
                break;

            default:
                break;
        }
    }

    void daemon_actions() {
        // 1. Lưu best solution từ generation hiện tại (đã bao gồm local search trên mỗi kiến)
        save_best_solution();

        if (!has_best_solution) return;

        // Tính tổng lợi nhuận của tất cả items (UB)
        long double UB = 0;
        for (int i = 0; i < nItems; ++i) {
            UB += items[i].profit;
        }
        
        long double fitness = UB + 1.0L - best_cost;
        
        // Global pheromone update
        long double d_tau = 1.0L / fitness;
        
        for (size_t i = 0; i + 1 < best_solution.sequence_len; ++i) {
            Pheromone.deposit_pheromone_on_the_visited_arc(
                best_solution.sequence_arr[i]->id,
                best_solution.sequence_arr[i+1]->id,
                d_tau);
        }
        
        Pheromone.check_pheromone_trail_limits(fitness);
    }

    void reset() {
        best_solution.restart();
        best_cost = -INF;
        has_best_solution = false;
    }
} Optional;
