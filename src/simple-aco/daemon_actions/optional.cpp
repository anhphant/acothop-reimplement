class Optional {
public:
    State best_solution;
    long double best_cost = -INF;
    bool has_best_solution = false;

    void save_best_solution() {
        for (auto& ant : Colony.ants) {
            if (ant.state.sequence.empty() || ant.state.sequence.back().id != componentList.back().id)
                continue;

            // ACO++ Feature: apply local search on each route constructed by the ants
            if (params.local_search_flag > 0) {
                switch (params.local_search_flag) {
                    case 1: two_opt(ant.state); break;
                    case 2: two_point_five_opt(ant.state); break;
                    case 3: three_opt(ant.state); break;
                }
            }

            long double cost = ant.state.solution_cost();

            if (!has_best_solution || cost > best_cost) {
                best_cost = cost;
                best_solution = ant.state;
                has_best_solution = true;
                log_debug("aco: new best solution found with cost ", cost,
                     " and ", best_solution.sequence.size(), " components");
            }
        }
    }

    void two_opt(State& s) {
        bool improved = true;
        while (improved) {
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

                    if (new_dist < current_dist) {
                        reverse(s.sequence.begin() + i, s.sequence.begin() + j + 1);
                        improved = true;
                    }
                }
            }
        }
    }

    void two_point_five_opt(State& s) {
        bool improved = true;
        while (improved) {
            improved = false;
            
            // 2-opt part
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
                    }
                }
            }

            // Node relocation (0.5 opt) part
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
                    }
                }
            }
        }
    }

    void three_opt(State& s) {
        two_point_five_opt(s);
        // Implement proper 3-opt if needed, but usually 2.5-opt is sufficient.
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