class Optional {
public:
    State best_solution;
    long double best_cost = 0;
    bool has_best_solution = false;
    int generations_without_improvement = 0;
    vector<pair<double, Ant*>> last_sorted_candidates;

    // Đánh giá siêu tốc tiềm năng TTP của một tour TSP (O(N))
    // Mô phỏng "vét ngược" từ đích về xuất phát để nhặt đồ ngon/nhẹ nhất
    double fast_ttp_proxy(const vector<int>& tour) {
        // Compute cumulative distances along the tour
        vector<double> dist_from_start(tour.size(), 0.0);
        for (size_t i = 1; i < tour.size(); ++i) {
            dist_from_start[i] = dist_from_start[i-1] + connections.distance(componentList[tour[i-1]], componentList[tour[i]]);
        }
        double total_dist = dist_from_start.back();

        // Actual TTP tour time budget (NOT the ACO runtime limit!)
        // budget = time remaining after running empty (max speed) along the tour
        double budget = maxTime - total_dist / maxSpeed;
        if (budget <= 0) return 0;

        double current_weight = 0;
        double profit = 0;

        // Late Harvest: traverse tour backwards, greedily pick profitable items
        // Items at city i are already sorted by p/w descending (done in packInit)
        for (int i = (int)tour.size() - 2; i >= 1; --i) {
            int city = tour[i];
            double d_to_end = total_dist - dist_from_start[i];

            for (int item : itemsAtCity[city]) {
                if (current_weight + items[item].weight > capacity) continue;

                double speed = maxSpeed - nu * current_weight;
                if (speed < minSpeed) speed = minSpeed;

                // MTC approximation: extra time cost of carrying this item to the end
                double added_time = (items[item].weight * nu * d_to_end) / (speed * speed);

                // Pick if: item doesn't blow the budget AND profit justifies time cost
                // Threshold: profit > added_time (i.e. profit rate > 1 per time unit)
                if (added_time < budget && (double)items[item].profit > added_time) {
                    budget -= added_time;
                    current_weight += items[item].weight;
                    profit += items[item].profit;
                }
            }
        }
        return profit;
    }

    void save_best_solution() {
        vector<pair<long double, Ant*>> candidates;
        candidates.reserve(Colony.ants.size());

        for (auto& ant : Colony.ants) {
            if (ant.state.sequence.empty() || ant.state.sequence.back().id != componentList.back().id)
                continue;

            // Dùng fast_ttp_proxy đánh giá siêu tốc tiềm năng TTP của tour
            vector<int> tour;
            tour.reserve(ant.state.sequence.size());
            for (auto& c : ant.state.sequence) tour.push_back(c.id);
            double quick_profit = fast_ttp_proxy(tour);
            candidates.push_back({quick_profit, &ant});
        }

        if (candidates.empty()) return;

        // Sắp xếp theo TTP Proxy thực tế (giúp DNA evolve đúng hướng TTP)
        sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
            return a.first > b.first;
        });

        last_sorted_candidates.clear();
        for (const auto& c : candidates) {
            last_sorted_candidates.push_back({(double)c.first, c.second});
        }

        // Chạy two_opt CHỈ TRÊN TOP 10 KIẾN (tiết kiệm 80% thời gian)
        double t1 = elapsed_time();
        if (params.local_search_flag > 0) {
            int max_idx = min(10, (int)candidates.size());
            for (int k = 0; k < max_idx; ++k) {
                Ant* ant = candidates[k].second;
                if (ant->state.sequence.empty() || ant->state.sequence.back().id != componentList.back().id) continue;
                
                two_opt(ant->state, 3);
                
                // Re-evaluate proxy after two_opt since tour distance changed
                vector<int> tour_ids;
                tour_ids.reserve(ant->state.sequence.size());
                for (auto& comp : ant->state.sequence) tour_ids.push_back(comp.id);
                
                candidates[k].first = fast_ttp_proxy(tour_ids);
            }
            
            // Re-sort the top 10 to find the true best after two_opt
            sort(candidates.begin(), candidates.begin() + max_idx, [](const auto& a, const auto& b) {
                return a.first > b.first;
            });
        }
        double t2 = elapsed_time();
        // printf("Time spent in two_opt: %f s\n", t2 - t1);

        // Deep polish CHỈ TRÊN BEST ANT
        Ant* best_ant = candidates[0].second;
        if (params.local_search_flag > 0) {
            switch (params.local_search_flag) {
                case 1: two_opt(best_ant->state, 30); break;
                case 2: two_point_five_opt(best_ant->state, 30); break;
                case 3: three_opt(best_ant->state, 30); break;
            }
        }

        // ONLY RUN REAL PACKING ON THE TOP 1 ANT!
        vector<int> best_tour;
        best_tour.reserve(best_ant->state.sequence.size());
        for (auto& c : best_ant->state.sequence) best_tour.push_back(c.id);

        double t3 = elapsed_time();
        Packing full_packing = pack(best_tour, packParams.ptries);
        double full_profit = computeFitness(best_tour, full_packing);
        double t4 = elapsed_time();
        printf("Time spent in pack(20): %f s\n", t4 - t3);

        // Thử reverse interior (giữ nguyên start/end) cho best ant
        vector<int> rev_tour = best_tour;
        reverse(rev_tour.begin() + 1, rev_tour.end() - 1);
        Packing rev_packing = pack(rev_tour, 3);
        double rev_profit = computeFitness(rev_tour, rev_packing);

        if (rev_profit > full_profit) {
            full_profit = rev_profit;
            full_packing = rev_packing;
            best_tour = rev_tour;
            // cập nhật lại state sequence
            vector<Component> new_seq;
            for (int id : rev_tour) new_seq.push_back(componentList[id]);
            best_ant->state.sequence = new_seq;
        }

        if (full_profit > best_cost) {
            best_cost = full_profit;
            best_solution = best_ant->state;
            has_best_solution = true;
            generations_without_improvement = 0;
            printf("[%.3fs] aco: new best solution found with cost %g and %zu components\n",
                elapsed_time(), (double)best_cost, best_solution.sequence.size());
        } else {
            generations_without_improvement++;
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
        
        // MMAS style: deposit on global best
        for (size_t i = 0; i + 1 < best_solution.sequence.size(); ++i) {
            Pheromone.deposit_pheromone_on_the_visited_arc(
                best_solution.sequence[i].id,
                best_solution.sequence[i+1].id,
                d_tau
            );
        }

        // Iteration-best deposit (top 1 ant of CURRENT generation)
        // Helps guide the search dynamically, preventing premature convergence
        if (!last_sorted_candidates.empty()) {
            Ant* iter_best_ant = last_sorted_candidates[0].second;
            if (!iter_best_ant->state.sequence.empty()) {
                for (size_t i = 0; i + 1 < iter_best_ant->state.sequence.size(); ++i) {
                    Pheromone.deposit_pheromone_on_the_visited_arc(
                        iter_best_ant->state.sequence[i].id,
                        iter_best_ant->state.sequence[i+1].id,
                        d_tau * 0.5L // 50% weight for iteration best
                    );
                }
            }
        }

        Pheromone.check_pheromone_trail_limits(best_cost);
    }

    void reset() {
        best_solution.restart();
        best_cost = -INF;
        has_best_solution = false;
    }
} Optional;