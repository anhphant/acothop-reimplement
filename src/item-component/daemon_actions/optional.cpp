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
                ant.state.recalculate();
                
                // Iteratively pack items and optimize time
                bool changed = true;
                while (changed) {
                    long double old_profit = ant.state.current_profit;
                    insert_items(ant.state);
                    two_point_five_opt(ant.state);
                    ant.state.recalculate();
                    if (ant.state.current_profit <= old_profit) {
                        changed = false;
                    }
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
        if (s.sequence.empty()) return;
        
        std::vector<int> tour;
        std::vector<std::vector<Component>> city_items(nCities);
        
        for (auto& c : s.sequence) {
            if (city_items[c.idCity].empty()) {
                tour.push_back(c.idCity);
            }
            city_items[c.idCity].push_back(c);
        }
        
        auto eval_tour_time = [&](const std::vector<int>& t) -> long double {
            long double current_time = 0.0L;
            long long current_weight = 0;
            int last_city = 0;
            for (int city : t) {
                current_time += ::distance(last_city, city) / (maxSpeed - current_weight * (maxSpeed - minSpeed) / capacity);
                for (auto& c : city_items[city]) {
                    current_weight += items[c.idItem].weight;
                }
                last_city = city;
            }
            current_time += ::distance(last_city, nCities - 1) / (maxSpeed - current_weight * (maxSpeed - minSpeed) / capacity);
            return current_time;
        };

        if (tour.size() <= 3) return;

        bool improved = true;
        while (improved) {
            improved = false;
            long double best_time = eval_tour_time(tour);
            
            for (size_t i = 1; i + 2 < tour.size(); ++i) {
                for (size_t j = i + 1; j + 1 < tour.size(); ++j) {
                    reverse(tour.begin() + i, tour.begin() + j + 1);
                    long double new_time = eval_tour_time(tour);
                    
                    if (new_time < best_time - 1e-6) {
                        best_time = new_time;
                        improved = true;
                    } else {
                        reverse(tour.begin() + i, tour.begin() + j + 1);
                    }
                }
            }
        }

        std::vector<Component> new_sequence;
        for (int city_id : tour) {
            for (auto& c : city_items[city_id]) {
                new_sequence.push_back(c);
            }
        }
        s.sequence = new_sequence;
    }

    void two_point_five_opt(State& s) {
        if (s.sequence.empty()) return;
        
        std::vector<int> tour;
        std::vector<std::vector<Component>> city_items(nCities);
        
        for (auto& c : s.sequence) {
            if (city_items[c.idCity].empty()) {
                tour.push_back(c.idCity);
            }
            city_items[c.idCity].push_back(c);
        }
        auto eval_tour_time = [&](const std::vector<int>& t) -> long double {
            long double current_time = 0.0L;
            long long current_weight = 0;
            int last_city = 0;
            for (int city : t) {
                current_time += ::distance(last_city, city) / (maxSpeed - current_weight * (maxSpeed - minSpeed) / capacity);
                for (auto& c : city_items[city]) {
                    current_weight += items[c.idItem].weight;
                }
                last_city = city;
            }
            current_time += ::distance(last_city, nCities - 1) / (maxSpeed - current_weight * (maxSpeed - minSpeed) / capacity);
            return current_time;
        };

        if (tour.size() <= 3) return;

        bool improved = true;
        while (improved) {
            improved = false;
            long double best_time = eval_tour_time(tour);
            
            // 2-opt part
            for (size_t i = 1; i + 2 < tour.size(); ++i) {
                for (size_t j = i + 1; j + 1 < tour.size(); ++j) {
                    reverse(tour.begin() + i, tour.begin() + j + 1);
                    long double new_time = eval_tour_time(tour);
                    
                    if (new_time < best_time - 1e-6) {
                        best_time = new_time;
                        improved = true;
                    } else {
                        reverse(tour.begin() + i, tour.begin() + j + 1);
                    }
                }
            }

            // Node relocation (0.5 opt) part
            for (size_t i = 1; i + 1 < tour.size(); ++i) {
                for (size_t j = 1; j + 1 < tour.size(); ++j) {
                    if (i == j || i == j + 1 || i + 1 == j) continue;

                    int temp = tour[i];
                    tour.erase(tour.begin() + i);
                    if (i < j) {
                        tour.insert(tour.begin() + j - 1, temp);
                    } else {
                        tour.insert(tour.begin() + j, temp);
                    }

                    long double new_time = eval_tour_time(tour);

                    if (new_time < best_time - 1e-6) {
                        best_time = new_time;
                        improved = true;
                    } else {
                        // Revert
                        tour.erase(tour.begin() + (i < j ? j - 1 : j));
                        tour.insert(tour.begin() + i, temp);
                    }
                }
            }
        }

        std::vector<Component> new_sequence;
        for (int city_id : tour) {
            for (auto& c : city_items[city_id]) {
                new_sequence.push_back(c);
            }
        }
        s.sequence = new_sequence;
    }

    void three_opt(State& s) {
        two_point_five_opt(s);
    }

    long double get_insertion_time(const State& s, size_t pos, const Component& c) const {
        long long next_weight = s.current_weight + items[c.idItem].weight;
        if (next_weight > capacity) return INF;

        long long cur_wt = 0;
        long double cur_tm = 0.0L;
        int last_city = 0;

        for (size_t i = 0; i < pos; ++i) {
            cur_tm += ::distance(last_city, s.sequence[i].idCity) / (maxSpeed - cur_wt * (maxSpeed - minSpeed) / capacity);
            cur_wt += items[s.sequence[i].idItem].weight;
            last_city = s.sequence[i].idCity;
        }

        cur_tm += ::distance(last_city, c.idCity) / (maxSpeed - cur_wt * (maxSpeed - minSpeed) / capacity);
        cur_wt += items[c.idItem].weight;
        last_city = c.idCity;

        for (size_t i = pos; i < s.sequence.size(); ++i) {
            cur_tm += ::distance(last_city, s.sequence[i].idCity) / (maxSpeed - cur_wt * (maxSpeed - minSpeed) / capacity);
            cur_wt += items[s.sequence[i].idItem].weight;
            last_city = s.sequence[i].idCity;
        }

        long double time_to_end = ::distance(last_city, nCities - 1) / (maxSpeed - cur_wt * (maxSpeed - minSpeed) / capacity);
        long double total_time = cur_tm + time_to_end;

        if (total_time > maxTime) return INF;
        return total_time;
    }

    void insert_items(State& s) {
        std::vector<Component> sorted_components = componentList;
        std::sort(sorted_components.begin(), sorted_components.end(), [](const Component& a, const Component& b) {
            long double p_a = items[a.idItem].profit;
            long double w_a = items[a.idItem].weight;
            long double p_b = items[b.idItem].profit;
            long double w_b = items[b.idItem].weight;
            return (p_a / w_a) > (p_b / w_b);
        });

        bool improved = true;
        while (improved) {
            improved = false;
            
            std::vector<bool> item_in_tour(items.size() + 1, false);
            std::vector<bool> city_in_tour(nCities, false);
            for (auto& sc : s.sequence) {
                item_in_tour[sc.idItem] = true;
                city_in_tour[sc.idCity] = true;
            }

            // Phase 1: Try to pack components into EXISTING cities in the tour
            for (auto& c : sorted_components) {
                if (item_in_tour[c.idItem] || !city_in_tour[c.idCity]) continue;

                int insert_pos = -1;
                for (int i = (int)s.sequence.size() - 1; i >= 0; --i) {
                    if (s.sequence[i].idCity == c.idCity) {
                        insert_pos = i + 1;
                        break;
                    }
                }

                if (insert_pos != -1) {
                    long double new_time = get_insertion_time(s, insert_pos, c);
                    if (new_time <= maxTime) {
                        s.sequence.insert(s.sequence.begin() + insert_pos, c);
                        s.recalculate();
                        improved = true;
                        
                        // Update lookups
                        item_in_tour[c.idItem] = true;
                    }
                }
            }

            if (improved) continue; // Keep packing existing cities until full

            // Phase 2: If we can't pack into existing cities, try to add ONE new city component
            std::vector<int> test_positions;
            test_positions.push_back(0);
            for (int i = 0; i < (int)s.sequence.size(); ++i) {
                if (i == (int)s.sequence.size() - 1 || s.sequence[i].idCity != s.sequence[i+1].idCity) {
                    test_positions.push_back(i + 1);
                }
            }

            std::vector<long double> prefix_time(s.sequence.size() + 1, 0.0L);
            std::vector<long long> prefix_weight(s.sequence.size() + 1, 0);
            std::vector<long double> suffix_dist(s.sequence.size() + 1, 0.0L);
            
            long long cur_wt = 0;
            long double cur_tm = 0.0L;
            int last_city = 0;
            for (size_t i = 0; i < s.sequence.size(); ++i) {
                prefix_time[i] = cur_tm;
                prefix_weight[i] = cur_wt;
                cur_tm += ::distance(last_city, s.sequence[i].idCity) / (maxSpeed - cur_wt * (maxSpeed - minSpeed) / capacity);
                cur_wt += items[s.sequence[i].idItem].weight;
                last_city = s.sequence[i].idCity;
            }
            prefix_time[s.sequence.size()] = cur_tm;
            prefix_weight[s.sequence.size()] = cur_wt;
            
            long double total_time = cur_tm + ::distance(last_city, nCities - 1) / (maxSpeed - cur_wt * (maxSpeed - minSpeed) / capacity);
            
            long double s_dist = ::distance(last_city, nCities - 1);
            suffix_dist[s.sequence.size()] = s_dist;
            for (int i = (int)s.sequence.size() - 1; i >= 0; --i) {
                int p_city = (i == 0) ? 0 : s.sequence[i-1].idCity;
                s_dist += ::distance(p_city, s.sequence[i].idCity);
                suffix_dist[i] = s_dist;
            }

            int best_comp_idx = -1;
            int best_insert_pos = -1;
            long double best_effective_density = -1.0L;
            int evaluated_count = 0;

            for (size_t c_idx = 0; c_idx < sorted_components.size(); ++c_idx) {
                auto& c = sorted_components[c_idx];
                if (item_in_tour[c.idItem] || city_in_tour[c.idCity]) continue;
                
                // Aggregate all unpicked items for c.idCity
                long double bundle_profit = 0;
                long long bundle_weight = 0;
                int first_c_idx = -1;
                
                for (size_t k = c_idx; k < sorted_components.size(); ++k) {
                    if (sorted_components[k].idCity == c.idCity && !item_in_tour[sorted_components[k].idItem]) {
                        if (s.current_weight + bundle_weight + items[sorted_components[k].idItem].weight <= capacity) {
                            bundle_profit += items[sorted_components[k].idItem].profit;
                            bundle_weight += items[sorted_components[k].idItem].weight;
                            if (first_c_idx == -1) first_c_idx = k;
                        }
                    }
                }
                
                if (first_c_idx == -1) continue; // nothing fits

                int best_pos = -1;
                long double best_approx_time = INF;
                for (int i : test_positions) {
                    int prev_city = (i == 0) ? 0 : s.sequence[i-1].idCity;
                    int next_city = (i == (int)s.sequence.size()) ? (nCities - 1) : s.sequence[i].idCity;
                    
                    long double speed_before = maxSpeed - prefix_weight[i] * (maxSpeed - minSpeed) / capacity;
                    long double speed_after = maxSpeed - (prefix_weight[i] + bundle_weight) * (maxSpeed - minSpeed) / capacity;
                    
                    long double time_at_next_old = ::distance(prev_city, next_city) / speed_before;
                    long double time_at_next_new = ::distance(prev_city, c.idCity) / speed_before + ::distance(c.idCity, next_city) / speed_after;
                    long double delta_time_local = time_at_next_new - time_at_next_old;
                    
                    long double current_suffix_time = total_time - prefix_time[i] - time_at_next_old;
                    if (current_suffix_time < 0) current_suffix_time = 0;
                    
                    long double remaining_dist = suffix_dist[i] - ::distance(prev_city, next_city);
                    if (remaining_dist < 0) remaining_dist = 0;

                    long double avg_speed_suffix = (current_suffix_time > 1e-9) ? remaining_dist / current_suffix_time : speed_before;
                    if (avg_speed_suffix > maxSpeed) avg_speed_suffix = maxSpeed;
                    
                    long double new_avg_speed_suffix = avg_speed_suffix - bundle_weight * (maxSpeed - minSpeed) / capacity;
                    if (new_avg_speed_suffix < minSpeed) new_avg_speed_suffix = minSpeed;
                    
                    long double new_suffix_time = remaining_dist / new_avg_speed_suffix;
                    
                    long double approx_time = total_time + delta_time_local + (new_suffix_time - current_suffix_time);
                    
                    if (approx_time < best_approx_time) {
                        best_approx_time = approx_time;
                        best_pos = i;
                    }
                }
                
                if (best_pos != -1) {
                    // Evaluate exact time of inserting ONLY the first item (Phase 1 will handle the rest)
                    long double exact_time = get_insertion_time(s, best_pos, sorted_components[first_c_idx]);
                    if (exact_time <= maxTime) {
                        // The true time increase includes the whole bundle eventually, but we just use the approx for density
                        long double time_increase = best_approx_time - s.current_time;
                        if (time_increase <= 1e-9) time_increase = 1e-9;
                        long double effective_density = bundle_profit / time_increase;

                        if (effective_density > best_effective_density) {
                            best_effective_density = effective_density;
                            best_insert_pos = best_pos;
                            best_comp_idx = first_c_idx;
                        }
                    }
                }

                evaluated_count++;

                if (evaluated_count >= 30) break; // Evaluate top 30 components
            }

            if (best_comp_idx != -1) {
                s.sequence.insert(s.sequence.begin() + best_insert_pos, sorted_components[best_comp_idx]);
                s.recalculate();
                improved = true;
            }
        }
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