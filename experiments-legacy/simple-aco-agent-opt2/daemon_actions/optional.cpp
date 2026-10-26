class Optional {
public:
    State best_solution;
    long double best_cost = -INF;
    bool has_best_solution = false;

    void save_best_solution() {
        for (auto& ant : Colony.ants) {
            if (ant.state.sequence.empty() || ant.state.sequence.back().id != componentList.back().id)
                continue;

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

    // --------------------------------------------------------
    // ESACO Strategy 1: Algorithm 2 - Cập nhật candidate sets
    // dựa trên thông tin pheromone sau mỗi generation.
    //
    // Với mỗi node n:
    //   1. Tìm best_j = argmax tau[n][j]
    //   2. Xóa phần tử cuối của nn_list[n], thêm best_j vào đầu
    //   3. Xóa phần tử cuối của nn_list[n], thêm successor của n
    //      trong best_solution vào đầu
    // Không cho phép trùng lặp.
    // --------------------------------------------------------
    void update_candidate_sets() {
        if (!has_best_solution) return;

        // Xây dựng bảng successor: successor[u] = v nếu trong best tour u→v
        vector<int> successor(nCities, -1);
        const auto& seq = best_solution.sequence;
        for (size_t k = 0; k + 1 < seq.size(); k++) {
            successor[seq[k].id] = seq[k + 1].id;
        }

        for (int n = 0; n < nCities; n++) {
            auto& cands = nn_list[n];
            if (cands.empty()) continue;

            // 1. Tìm node có pheromone cao nhất từ n (từ sparse list)
            int best_j = Pheromone.get_best_pheromone_neighbor(n);
            if (best_j != -1 && best_j != n) {
                // Xóa nếu đã có trong list
                cands.erase(remove(cands.begin(), cands.end(), best_j), cands.end());
                // Xóa phần tử cuối nếu đầy (để giữ kích thước không đổi)
                if (!cands.empty()) cands.pop_back();
                // Thêm vào đầu
                cands.insert(cands.begin(), best_j);
            }

            // 2. Successor của n trong best tour
            int succ = successor[n];
            if (succ != -1 && succ != n) {
                cands.erase(remove(cands.begin(), cands.end(), succ), cands.end());
                if (!cands.empty()) cands.pop_back();
                cands.insert(cands.begin(), succ);
            }
        }
    }

    void two_opt(State& s) {
        bool improved = true;
        
        vector<int> pos(nCities, -1);
        for (size_t k = 0; k < s.sequence.size(); k++) {
            pos[s.sequence[k].id] = k;
        }

        while (improved) {
            improved = false;
            for (size_t i = 0; i + 2 < s.sequence.size(); i++) {
                int u_id = s.sequence[i].id;
                
                for (int v_id : nn_list[u_id]) {
                    int j = pos[v_id];
                    if (j == -1) continue;
                    
                    if (j > i + 1 && j + 1 < s.sequence.size()) {
                        if (connections.distance(s.sequence[i], s.sequence[j]) +
                            connections.distance(s.sequence[i+1], s.sequence[j+1]) <
                            connections.distance(s.sequence[i], s.sequence[i+1]) +
                            connections.distance(s.sequence[j], s.sequence[j+1]) - 1e-9) {
                            
                            reverse(s.sequence.begin()+i+1, s.sequence.begin()+j+1);
                            
                            for (size_t k = i + 1; k <= j; k++) {
                                pos[s.sequence[k].id] = k;
                            }
                            
                            improved = true;
                        }
                    }
                }
            }
        }
    }

    void two_point_five_opt(State& s) {
        bool improved = true;
        vector<int> pos(nCities, -1);
        
        while (improved) {
            two_opt(s);
            improved = false;
            
            for (size_t k = 0; k < s.sequence.size(); k++) {
                pos[s.sequence[k].id] = k;
            }
            
            for (size_t i = 0; i + 1 < s.sequence.size(); i++) {
                int u_id = s.sequence[i].id;
                int next_id = s.sequence[i+1].id;
                
                auto try_j = [&](int v_id) -> bool {
                    int j = pos[v_id];
                    if (j == -1 || j == 0 || j + 1 >= (int)s.sequence.size()) return false;
                    if (j == (int)i || j == (int)i+1) return false;
                    
                    double d_old = connections.distance(s.sequence[j-1], s.sequence[j]) +
                                   connections.distance(s.sequence[j], s.sequence[j+1]) +
                                   connections.distance(s.sequence[i], s.sequence[i+1]);
                                   
                    double d_new = connections.distance(s.sequence[j-1], s.sequence[j+1]) +
                                   connections.distance(s.sequence[i], s.sequence[j]) +
                                   connections.distance(s.sequence[j], s.sequence[i+1]);
                                   
                    if (d_new < d_old - 1e-9) {
                        auto x = s.sequence[j];
                        s.sequence.erase(s.sequence.begin()+j);
                        s.sequence.insert(s.sequence.begin()+i+(j>i), x);
                        
                        int start = min((int)i, j);
                        int end = max((int)i+1, j);
                        for (int k = start; k <= end; k++) {
                            pos[s.sequence[k].id] = k;
                        }
                        return true;
                    }
                    return false;
                };

                bool found = false;
                for (int v_id : nn_list[u_id]) {
                    if (try_j(v_id)) { found = true; break; }
                }
                if (found) { improved = true; continue; }
                
                for (int v_id : nn_list[next_id]) {
                    if (try_j(v_id)) { found = true; break; }
                }
                if (found) { improved = true; }
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

    void daemon_actions(int generation) {
        auto t0 = chrono::steady_clock::now();
        // 1. Tìm iteration_best_solution từ current ants (có local search cho mỗi ant)
        State iteration_best_solution;
        long double iteration_best_cost = -INF;
        bool has_iteration_best = false;
        
        double total_ms_ls = 0;
        double total_ms_ls_pack = 0;

        for (auto& ant : Colony.ants) {
            if (ant.state.sequence.empty() || ant.state.sequence.back().id != componentList.back().id)
                continue;

            // ESACO Strategy 2 / ACO++ Feature: apply local search on each route
            if (params.local_search_flag > 0) {
                auto t_ls_start = chrono::steady_clock::now();
                switch (params.local_search_flag) {
                    case 1: two_opt(ant.state); break;
                    case 2: two_point_five_opt(ant.state); break;
                    case 3: three_opt(ant.state); break;
                }
                auto t_ls_end = chrono::steady_clock::now();
                total_ms_ls += chrono::duration<double, milli>(t_ls_end - t_ls_start).count();
            }

            auto t_pack_start = chrono::steady_clock::now();
            long double cost = ant.state.solution_cost();
            auto t_pack_end = chrono::steady_clock::now();
            total_ms_ls_pack += chrono::duration<double, milli>(t_pack_end - t_pack_start).count();

            if (!has_iteration_best || cost > iteration_best_cost) {
                iteration_best_cost = cost;
                iteration_best_solution = ant.state;
                has_iteration_best = true;
            }
        }
        auto t1 = chrono::steady_clock::now();
        
        if (!has_iteration_best) return;

        double ms_pack_all = chrono::duration<double, milli>(t1 - t0).count();
        log_debug("daemon timings: pack_all=", ms_pack_all, "ms, all_ants_ls=", total_ms_ls, "ms, all_ants_ls_pack=", total_ms_ls_pack, "ms");

        // 3. Cập nhật global best
        if (!has_best_solution || iteration_best_cost > best_cost) {
            best_cost = iteration_best_cost;
            best_solution = iteration_best_solution;
            has_best_solution = true;
            log_debug("aco: new best solution found with cost ", best_cost,
                 " and ", best_solution.sequence.size(), " components");
        }

        // 4. MMAS Schedule: Determine which ant deposits pheromone
        int u_gb = 1;
        if (generation <= 25) u_gb = 25;
        else if (generation <= 75) u_gb = 5;
        else if (generation <= 125) u_gb = 3;
        else if (generation <= 250) u_gb = 2;
        else u_gb = 1;
        
        State* depositing_ant = nullptr;
        if (generation % u_gb != 0) {
            depositing_ant = &iteration_best_solution;
        } else {
            depositing_ant = &best_solution;
        }

        // Tính tổng lợi nhuận của tất cả items (UB) để tính fitness
        long double UB = 0;
        for (int i = 0; i < nItems; ++i) {
            UB += items[i].profit;
        }
        
        // Theo chuẩn MMAS thop.c: fitness = UB + 1 - profit
        // Tính fitness cho ant đang được dùng để rải pheromone
        long double depositing_cost = depositing_ant->cost;
        long double depositing_fitness = UB + 1.0L - depositing_cost;
        long double d_tau = 1.0L / depositing_fitness;
        
        for (size_t i = 0; i + 1 < depositing_ant->sequence.size(); ++i) {
            Pheromone.deposit_pheromone_on_the_visited_arc(
                depositing_ant->sequence[i].id,
                depositing_ant->sequence[i+1].id,
                d_tau);
        }
        
        // trail_max luôn dựa trên global best_cost
        long double global_fitness = UB + 1.0L - best_cost;
        Pheromone.check_pheromone_trail_limits(global_fitness);

        // ESACO Strategy 1: cập nhật candidate sets sau khi pheromone đã được update
        // Chỉ cập nhật mỗi 50 generation để tránh premature convergence
        if (generation % 50 == 0) {
            update_candidate_sets();
        }
    }

    void reset() {
        best_solution.restart();
        best_cost = -INF;
        has_best_solution = false;
    }
} Optional;