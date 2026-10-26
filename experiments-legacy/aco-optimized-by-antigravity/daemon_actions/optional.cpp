class Optional {
public:
    State best_solution;
    long double best_cost = -INF;
    bool has_best_solution = false;
    int stagnation_counter = 0;

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

    void fast_two_opt(State& s) {
        bool improved = true;
        int n = s.sequence.size();
        vector<bool> dont_look(nCities, false);
        vector<int> pos(nCities, -1);
        for(int i = 0; i < n; ++i) pos[s.sequence[i].id] = i;
        
        while(improved) {
            improved = false;
            for (int i = 0; i < n - 1; ++i) {
                int u1 = s.sequence[i].id;
                if (dont_look[u1]) continue;
                
                bool node_improved = false;
                int u2 = s.sequence[i+1].id;
                
                for (int v1 : candidateList[u1]) {
                    int j = pos[v1];
                    if (j == -1 || j <= i + 1 || j >= n - 1) continue;
                    
                    int v2 = s.sequence[j+1].id;
                    double d_old = connections.distance(componentList[u1], componentList[u2]) + connections.distance(componentList[v1], componentList[v2]);
                    double d_new = connections.distance(componentList[u1], componentList[v1]) + connections.distance(componentList[u2], componentList[v2]);
                    
                    if (d_new < d_old - 1e-6) {
                        State t = s;
                        reverse(t.sequence.begin() + i + 1, t.sequence.begin() + j + 1);
                        
                        if (params.two_opt_eval_mode == 0) {
                            t.cost_computed = false;
                            s = t;
                            for (int k = i + 1; k <= j; ++k) pos[s.sequence[k].id] = k;
                            dont_look[u1] = false; dont_look[u2] = false;
                            dont_look[v1] = false; dont_look[v2] = false;
                            node_improved = true;
                            improved = true;
                            break;
                        } else if (params.two_opt_eval_mode == 1) {
                            vector<int> t_tour;
                            for (auto c : t.sequence) t_tour.push_back(c.id);
                            Packing temp_packing = s.cached_packing;
                            long double new_cost = computeFitness(t_tour, temp_packing);
                            if (new_cost > s.cached_cost) {
                                t.cached_cost = new_cost;
                                t.cached_packing = temp_packing;
                                t.cost_computed = true;
                                s = t;
                                for (int k = i + 1; k <= j; ++k) pos[s.sequence[k].id] = k;
                                dont_look[u1] = false; dont_look[u2] = false;
                                dont_look[v1] = false; dont_look[v2] = false;
                                node_improved = true;
                                improved = true;
                                break;
                            }
                        } else if (params.two_opt_eval_mode == 2) {
                            t.cost_computed = false;
                            if (t.solution_cost() > s.solution_cost()) {
                                s = t;
                                for (int k = i + 1; k <= j; ++k) pos[s.sequence[k].id] = k;
                                dont_look[u1] = false; dont_look[u2] = false;
                                dont_look[v1] = false; dont_look[v2] = false;
                                node_improved = true;
                                improved = true;
                                break;
                            }
                        }
                    }
                }
                
                if (!node_improved) dont_look[u1] = true;
            }
        }
    }

    void double_bridge_move(State& s) {
        int n = s.sequence.size();
        if (n < 8) return;
        int p1 = 1 + rand() % (n / 4);
        int p2 = p1 + 1 + rand() % (n / 4);
        int p3 = p2 + 1 + rand() % (n / 4);
        if (p3 >= n - 1) return;
        
        State t = s;
        t.sequence.clear();
        t.sequence.insert(t.sequence.end(), s.sequence.begin(), s.sequence.begin() + p1);
        t.sequence.insert(t.sequence.end(), s.sequence.begin() + p3, s.sequence.end() - 1);
        t.sequence.insert(t.sequence.end(), s.sequence.begin() + p2, s.sequence.begin() + p3);
        t.sequence.insert(t.sequence.end(), s.sequence.begin() + p1, s.sequence.begin() + p2);
        t.sequence.push_back(s.sequence.back());
        
        t.cost_computed = false;
        fast_two_opt(t); // optimize the perturbed state
        if (t.solution_cost() > s.solution_cost()) {
            s = t;
        }
    }

    void heavy_early_penalty_swap(State& s) {
        int n = s.sequence.size();
        if (n < 2) return;
        
        int early_limit = n * params.heavy_early_threshold;
        int late_start = n * params.heavy_late_threshold;
        if (early_limit <= 0) early_limit = 1;
        
        vector<int> tour;
        for (auto c : s.sequence) tour.push_back(c.id);
        
        State t = s;
        bool changed = false;
        
        for (int iter = 0; iter < 100; ++iter) {
            State temp = t;
            int p1 = rand() % early_limit;
            int city1 = tour[p1];
            if (!itemsAtCity[city1].empty()) {
                int item1 = itemsAtCity[city1][rand() % itemsAtCity[city1].size()];
                if (temp.cached_packing.picked[item1]) {
                    temp.cached_packing.picked[item1] = false;
                    
                    if (late_start < n) {
                        int p2 = late_start + rand() % (n - late_start);
                        int city2 = tour[p2];
                        if (!itemsAtCity[city2].empty()) {
                            int item2 = itemsAtCity[city2][rand() % itemsAtCity[city2].size()];
                            if (!temp.cached_packing.picked[item2]) {
                                temp.cached_packing.picked[item2] = true;
                            }
                        }
                    }
                    
                    long double new_cost = computeFitness(tour, temp.cached_packing);
                    if (new_cost > t.cached_cost) {
                        t = temp;
                        t.cached_cost = new_cost;
                        changed = true;
                    }
                }
            }
        }
        if (changed) {
            s = t;
            s.cost_computed = true;
        }
    }

    void late_node_insertion(State& s) {
        int n = s.sequence.size();
        if (n < 4) return;
        
        double original_dist = 0;
        for (size_t i = 0; i < n - 1; ++i) {
            original_dist += connections.distance(componentList[s.sequence[i].id], componentList[s.sequence[i+1].id]);
        }
        
        vector<int> pos(nCities, -1);
        for (int i = 0; i < n; ++i) pos[s.sequence[i].id] = i;
        
        bool changed = false;
        int half = n / 2;
        
        // 1. Candidate-Guided Elite City Relocation
        int late_start = n - n / 5;
        if (late_start <= half) late_start = half + 1;
        
        for (int p1 = 1; p1 < half; ++p1) {
            int u1 = s.sequence[p1].id;
            int city_idx = cityIdToNodeIdx[u1];
            
            if (city_idx != -1 && is_elite_city[city_idx]) {
                int best_p2 = -1;
                double best_new_dist = INF;
                State best_t = s;
                
                for (int v1 : candidateList[u1]) {
                    int p2 = pos[v1];
                    if (p2 >= late_start && p2 < n - 2) {
                        State t = s;
                        auto x = t.sequence[p1];
                        t.sequence.erase(t.sequence.begin() + p1);
                        t.sequence.insert(t.sequence.begin() + p2, x);
                        
                        double new_dist = 0;
                        for (size_t i = 0; i < n - 1; ++i) {
                            new_dist += connections.distance(componentList[t.sequence[i].id], componentList[t.sequence[i+1].id]);
                        }
                        
                        if (new_dist < best_new_dist) {
                            best_new_dist = new_dist;
                            best_t = t;
                            best_p2 = p2;
                        }
                    }
                }
                
                if (best_p2 != -1 && best_new_dist < original_dist + 1e-4) {
                    s = best_t;
                    original_dist = best_new_dist;
                    changed = true;
                    for (int i = 0; i < n; ++i) pos[s.sequence[i].id] = i;
                }
            }
        }
        
        // 2. Candidate-Guided Random Relocation
        for (int iter = 0; iter < 100; ++iter) {
            int p1 = 1 + rand() % half;
            int u1 = s.sequence[p1].id;
            
            int best_p2 = -1;
            double best_new_dist = INF;
            State best_t = s;
            
            for (int v1 : candidateList[u1]) {
                int p2 = pos[v1];
                if (p2 > half && p2 < n - 2) {
                    State t = s;
                    auto x = t.sequence[p1];
                    t.sequence.erase(t.sequence.begin() + p1);
                    t.sequence.insert(t.sequence.begin() + p2, x);
                    
                    double new_dist = 0;
                    for (size_t i = 0; i < n - 1; ++i) {
                        new_dist += connections.distance(componentList[t.sequence[i].id], componentList[t.sequence[i+1].id]);
                    }
                    
                    if (new_dist < best_new_dist) {
                        best_new_dist = new_dist;
                        best_t = t;
                        best_p2 = p2;
                    }
                }
            }
            
            if (best_p2 != -1 && best_new_dist < original_dist - 1e-6) {
                s = best_t;
                original_dist = best_new_dist;
                changed = true;
                for (int i = 0; i < n; ++i) pos[s.sequence[i].id] = i;
            }
        }
        
        if (changed) {
            s.cost_computed = false;
        }
    }

    void daemon_actions() {
        State* iteration_best = nullptr;
        long double iteration_best_cost = -INF;
        
        // 1. Sort ants by raw routing distance
        vector<pair<double, Ant*>> sorted_ants;
        for (auto& ant : Colony.ants) {
            if (ant.state.sequence.empty() || ant.state.sequence.back().id != componentList.back().id) continue;
            
            double dist = 0;
            for (size_t i = 0; i + 1 < ant.state.sequence.size(); ++i) {
                dist += connections.distance(componentList[ant.state.sequence[i].id], componentList[ant.state.sequence[i+1].id]);
            }
            sorted_ants.push_back({dist, &ant});
        }
        sort(sorted_ants.begin(), sorted_ants.end());
        
        // 2. Only run full EA on the Top 10% ants (max 20)
        int elite_count = max(1, min((int)sorted_ants.size(), (int)(sorted_ants.size() * 0.10)));
        for (int i = 0; i < sorted_ants.size(); ++i) {
            Ant* ant = sorted_ants[i].second;
            State* state = &ant->state;
            if (i < elite_count) {
                long double cost = state->solution_cost(); // Runs full EA
                if (iteration_best == nullptr || cost > iteration_best_cost) {
                    iteration_best_cost = cost;
                    iteration_best = state;
                }
                // Flag top 5% as elite to preserve them for the next generation
                if (i < max(1, elite_count / 2)) {
                    ant->is_elite = true;
                }
            }
        }
        
        if (iteration_best != nullptr) {
            // ALWAYS run Local Search on the iteration best to maximize exploitation
            fast_two_opt(*iteration_best);
            heavy_early_penalty_swap(*iteration_best);
            late_node_insertion(*iteration_best);
            
            // Periodically perturb to escape local optima
            if (rand() % 4 == 0) {
                double_bridge_move(*iteration_best);
            }
            
            // Save global best
            long double improved_cost = iteration_best->solution_cost();
            if (!has_best_solution || improved_cost > best_cost) {
                best_cost = improved_cost;
                best_solution = *iteration_best;
                has_best_solution = true;
                stagnation_counter = 0;
                log_debug("aco: new best solution found with cost ", improved_cost,
                     " and ", best_solution.sequence.size(), " components");
                     
                // Dynamic MMAS bounds based on best cost
                Pheromone.tau_max = max(1.0L, best_cost) / 100.0L;
                Pheromone.tau_min = Pheromone.tau_max / 2000.0L;
            } else {
                stagnation_counter++;
            }
            
            if (stagnation_counter >= params.stagnation_threshold) {
                log_debug("aco: stagnation reached (", stagnation_counter, " gens), restarting pheromone!");
                Pheromone.reset_all();
                stagnation_counter = 0;
            }
            
            // Global Pheromone Update on the highly optimized iteration best
            for (size_t i = 0; i + 1 < iteration_best->sequence.size(); ++i) {
                Pheromone.deposit_pheromone_on_the_visited_arc(
                    iteration_best->sequence[i].id,
                    iteration_best->sequence[i + 1].id
                );
            }
        }
    }

    void reset() {
        best_solution.restart();
        best_cost = -INF;
        has_best_solution = false;
    }

    void save_solution_to_file(const string& filename) {
        if (!has_best_solution) return;
        
        // Re-run pack() on best_solution.sequence to ensure 100% perfect sync
        vector<int> tour;
        for (auto c : best_solution.sequence) tour.push_back(c.id);
        best_solution.cached_packing = pack(tour);
        
        ofstream out(filename);
        if (!out) return;
        
        // Find which cities have picked items
        vector<bool> city_has_picked(nCities, false);
        for (size_t i = 0; i < best_solution.cached_packing.picked.size(); ++i) {
            if (best_solution.cached_packing.picked[i]) {
                city_has_picked[items[i].city] = true;
            }
        }
        
        // Print intermediate tour (excluding start and end)
        out << "[";
        bool first = true;
        for (size_t i = 1; i < best_solution.sequence.size() - 1; ++i) {
            int city_id = best_solution.sequence[i].id;
            if (city_has_picked[city_id]) {
                if (!first) out << ", ";
                out << city_id + 1;
                first = false;
            }
        }
        out << "]\n";
        
        // Print picked items
        out << "[";
        first = true;
        for (size_t i = 0; i < best_solution.cached_packing.picked.size(); ++i) {
            if (best_solution.cached_packing.picked[i]) {
                if (!first) out << ", ";
                out << items[i].id + 1;
                first = false;
            }
        }
        out << "]\n";
        out.close();
    }
} Optional;