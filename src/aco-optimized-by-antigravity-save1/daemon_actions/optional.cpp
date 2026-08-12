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
                        reverse(s.sequence.begin() + i + 1, s.sequence.begin() + j + 1);
                        s.cost_computed = false;
                        for (int k = i + 1; k <= j; ++k) pos[s.sequence[k].id] = k;
                        dont_look[u1] = false; dont_look[u2] = false;
                        dont_look[v1] = false; dont_look[v2] = false;
                        node_improved = true;
                        improved = true;
                        break;
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
        
        State t = s;
        t.sequence.clear();
        t.sequence.insert(t.sequence.end(), s.sequence.begin(), s.sequence.begin() + p1);
        t.sequence.insert(t.sequence.end(), s.sequence.begin() + p3, s.sequence.end());
        t.sequence.insert(t.sequence.end(), s.sequence.begin() + p2, s.sequence.begin() + p3);
        t.sequence.insert(t.sequence.end(), s.sequence.begin() + p1, s.sequence.begin() + p2);
        
        t.cost_computed = false;
        fast_two_opt(t); // optimize the perturbed state
        if (t.solution_cost() > s.solution_cost()) {
            s = t;
        }
    }

    void late_node_insertion(State& s) {
        int n = s.sequence.size();
        if (n < 4) return;
        for (int iter = 0; iter < 100; ++iter) {
            int p1 = 1 + rand() % (n / 2); // grab from first half
            int p2 = n / 2 + rand() % (n / 2 - 1); // insert to second half
            if (p1 == p2) continue;
            
            State t = s;
            auto x = t.sequence[p1];
            t.sequence.erase(t.sequence.begin() + p1);
            t.sequence.insert(t.sequence.begin() + p2, x);
            t.cost_computed = false;
            
            if (t.solution_cost() > s.solution_cost()) {
                s = t;
            }
        }
    }

    void daemon_actions() {
        State* iteration_best = nullptr;
        long double iteration_best_cost = -INF;
        
        for (auto& ant : Colony.ants) {
            if (ant.state.sequence.empty() || ant.state.sequence.back().id != componentList.back().id) continue;
            long double cost = ant.state.solution_cost();
            if (iteration_best == nullptr || cost > iteration_best_cost) {
                iteration_best_cost = cost;
                iteration_best = &ant.state;
            }
        }
        
        if (iteration_best != nullptr) {
            // ALWAYS run Local Search on the iteration best to maximize exploitation
            fast_two_opt(*iteration_best);
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
                log_debug("aco: new best solution found with cost ", improved_cost,
                     " and ", best_solution.sequence.size(), " components");
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
} Optional;