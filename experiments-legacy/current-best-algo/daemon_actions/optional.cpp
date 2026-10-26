class Optional {
public:
    State best_solution;
    long double best_cost = -INF;
    bool has_best_solution = false;

    void save_best_solution() {
        Ant* iter_best_ant = nullptr;
        long double iter_best_cost = -INF;
        
        vector<pair<long double, Ant*>> candidates;
        candidates.reserve(Colony.ants.size());

        for (auto& ant : Colony.ants) {
            if (ant.state.sequence.empty() || ant.state.sequence.back().id != componentList.back().id)
                continue;

            // Ultra-fast 2-opt sweep (3 passes) on all ants
            if (params.local_search_flag > 0) {
                two_opt(ant.state, 3);
            }
                
            // Compute pure geometric TSP distance in O(N)
            long double tsp_dist = 0.0L;
            for (size_t k = 0; k + 1 < ant.state.sequence.size(); ++k) {
                tsp_dist += connections.distance(ant.state.sequence[k], ant.state.sequence[k+1]);
            }
            candidates.push_back({tsp_dist, &ant});
        }

        if (candidates.empty()) return;

        // Sort by shortest TSP distance first
        sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });

        // Deep polish the best candidate
        if (params.local_search_flag > 0) {
            switch (params.local_search_flag) {
                case 1: two_opt(candidates[0].second->state, 30); break;
                case 2: two_point_five_opt(candidates[0].second->state, 30); break;
                case 3: three_opt(candidates[0].second->state, 30); break;
            }
        }

        // Only evaluate expensive Knapsack Packing on the Top K candidates (K = 5)
        size_t top_k = min((size_t)5, candidates.size());
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
                log_debug("aco: new best solution found with cost ", cost,
                     " and ", best_solution.sequence.size(), " components");
            }
        }
    }

    void two_opt(State& s, int max_passes = 20) {
        bool improved = true;
        int pass = 0;
        while (improved && ++pass <= max_passes) {
            improved = false;
            for (size_t i = 1; i + 2 < s.sequence.size(); ++i) {
                for (size_t j = i + 1; j + 1 < s.sequence.size(); ++j) {
                    int u_id = s.sequence[i-1].id;
                    int v_id = s.sequence[i].id;
                    int x_id = s.sequence[j].id;
                    int y_id = s.sequence[j+1].id;

                    long double current_dist = connections.distance(componentList[u_id], componentList[v_id]) + 
                                               connections.distance(componentList[x_id], componentList[y_id]);
                    long double new_dist = connections.distance(componentList[u_id], componentList[x_id]) + 
                                           connections.distance(componentList[v_id], componentList[y_id]);

                    if (new_dist < current_dist - 1e-6) {
                        reverse(s.sequence.begin() + i, s.sequence.begin() + j + 1);
                        improved = true;
                        break;
                    }
                }
            }
        }
    }

    void two_point_five_opt(State& s, int max_passes = 15) {
        bool improved = true;
        int pass = 0;
        while (improved && ++pass <= max_passes) {
            improved = false;
            // 2-opt sweep
            for (size_t i = 1; i + 2 < s.sequence.size(); ++i) {
                for (size_t j = i + 1; j + 1 < s.sequence.size(); ++j) {
                    int u_id = s.sequence[i-1].id;
                    int v_id = s.sequence[i].id;
                    int x_id = s.sequence[j].id;
                    int y_id = s.sequence[j+1].id;

                    long double current_dist = connections.distance(componentList[u_id], componentList[v_id]) + 
                                               connections.distance(componentList[x_id], componentList[y_id]);
                    long double new_dist = connections.distance(componentList[u_id], componentList[x_id]) + 
                                           connections.distance(componentList[v_id], componentList[y_id]);

                    if (new_dist < current_dist - 1e-6) {
                        reverse(s.sequence.begin() + i, s.sequence.begin() + j + 1);
                        improved = true;
                        break;
                    }
                }
            }

            // Node relocation (0.5 opt) sweep
            for (size_t i = 1; i + 1 < s.sequence.size(); ++i) {
                for (size_t j = 1; j + 1 < s.sequence.size(); ++j) {
                    if (i == j || i == j + 1 || i + 1 == j) continue;

                    int u_id = s.sequence[i-1].id;
                    int v_id = s.sequence[i].id;
                    int w_id = s.sequence[i+1].id;

                    int x_id = s.sequence[j-1].id;
                    int y_id = s.sequence[j].id;

                    long double current_dist = connections.distance(componentList[u_id], componentList[v_id]) + 
                                               connections.distance(componentList[v_id], componentList[w_id]) + 
                                               connections.distance(componentList[x_id], componentList[y_id]);
                    
                    long double new_dist = connections.distance(componentList[u_id], componentList[w_id]) + 
                                           connections.distance(componentList[x_id], componentList[v_id]) + 
                                           connections.distance(componentList[v_id], componentList[y_id]);

                    if (new_dist < current_dist - 1e-6) {
                        Component temp = s.sequence[i];
                        s.sequence.erase(s.sequence.begin() + i);
                        if (i < j) {
                            s.sequence.insert(s.sequence.begin() + j - 1, temp);
                        } else {
                            s.sequence.insert(s.sequence.begin() + j, temp);
                        }
                        improved = true;
                        break;
                    }
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
        
        for (size_t i = 0; i + 1 < best_solution.sequence.size(); ++i) {
            Pheromone.deposit_pheromone_on_the_visited_arc(
                best_solution.sequence[i].id,
                best_solution.sequence[i+1].id,
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