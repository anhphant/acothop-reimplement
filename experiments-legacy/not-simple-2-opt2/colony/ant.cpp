struct Ant {
    int sub_idx = 0;
    struct Memory {
        bool visited_components[MAX_CITIES];

        void reset(int n) {
            memset(visited_components, 0, n);
        }

        bool visited(int id) const {
            return visited_components[id];
        }

        void visit(int id) {
            visited_components[id] = true;
        }
    };

    State state;
    Memory memory;

    /*------------------------------------------------------------*/

    void initialize(int numberOfComponents) {
        state.sequenceSize = 0;
        memory.reset(numberOfComponents);
        memory.visit(nCities);   // dummy node (id == nCities) is never a candidate
    }

    bool termination_condition() const {
        if (state.sequenceSize >= nCities) return true;
        if (state.sequenceSize > 0 && state.sequence[state.sequenceSize - 1] == nCities - 1) return true;
        return false;
    }

    /*------------------------------------------------------------*/

    void visit_component(const Component& component) {
        state.sequence[state.sequenceSize++] = component.id;
        memory.visit(component.id);
    }

    double transition_score(const Component& from, const Component& to) const {
        return Pheromone.getTotalArbitrary(from.id, to.id, sub_idx);
    }

    /*------------------------------------------------------------*/

    // Faithful port of realbench choose_best_next (ants.c:302): pick the
    // unvisited city with maximal total[pheromone*heuristic].
    void choose_best_next() {
        const Component& current = componentList[state.sequence[state.sequenceSize - 1]];
        
        static double prob_ptr[MAX_CITIES];
        static int unvisited_cities[MAX_CITIES];
        int unvisited_count = 0;
        double sum_prob = 0.0;
        
        for (int city = 0; city < nCities; ++city) {
            if (memory.visited(city)) continue;
            double val = transition_score(current, componentList[city]);
            prob_ptr[unvisited_count] = val;
            unvisited_cities[unvisited_count] = city;
            sum_prob += val;
            unvisited_count++;
        }
        
        if (sum_prob > 0.0) {
            double rnd = ran01(&seed) * sum_prob;
            int i = 0;
            double partial_sum = prob_ptr[i];
            while (partial_sum <= rnd && i < unvisited_count - 1) {
                i++;
                partial_sum += prob_ptr[i];
            }
            visit_component(componentList[unvisited_cities[i]]);
            return;
        }

        // Fallback to greedy if sum_prob == 0 (e.g. all values underflowed)
        int next_city = unvisited_cities[0];
        double value_best = prob_ptr[0];
        for (int i = 1; i < unvisited_count; ++i) {
            if (prob_ptr[i] > value_best) {
                value_best = prob_ptr[i];
                next_city = unvisited_cities[i];
            }
        }
        visit_component(componentList[next_city]);
    }

    void neighbour_choose_best_next() {
        const Component& current = componentList[state.sequence[state.sequenceSize - 1]];
        int next_city = nCities + 1;
        double value_best = -1.0;
        
        // Tầng 1: primary knn (rank 1-20)
        for (int i = 0; i < NN_ANTS; ++i) {
            int help_city = knn[current.id][i];
            if (memory.visited(help_city)) continue;
            double help = Pheromone.getTotalNN(current.id, i, sub_idx);
            if (help > value_best) { value_best = help; next_city = help_city; }
        }
        if (next_city != nCities + 1) {
            visit_component(componentList[next_city]);
            return;
        }

        // Tầng 2: backup knn (rank 21-40)
        for (int i = 0; i < NN_BACKUP_ANTS; ++i) {
            int help_city = knn_backup[current.id][i];
            if (memory.visited(help_city)) continue;
            double help = Pheromone.getTotalArbitrary(current.id, help_city, sub_idx);
            if (help > value_best) { value_best = help; next_city = help_city; }
        }
        if (next_city != nCities + 1) {
            visit_component(componentList[next_city]);
            return;
        }

        // Tầng 3: fallback
        choose_best_next();
    }

    /*------------------------------------------------------------*/

    // Faithful port of realbench neighbour_choose_and_move_to_next (ants.c:405).
    // q_0 == 0.0 for MMAS, so the ACS exploitation branch never consumes RNG.
    void move_next() {
        if (nCities == 0)
            return;

        if (termination_condition())
            return;

        // Start node
        if (state.sequenceSize == 0) {
            visit_component(componentList[0]);
            return;
        }

        const int phase = state.sequenceSize;
        const Component& current = componentList[state.sequence[state.sequenceSize - 1]];

        static double prob_ptr[MAX_CITIES + 1];
        double sum_prob = 0.0;
        for (int i = 0; i < NN_ANTS; ++i) {
            int help_city = knn[current.id][i];
            if (memory.visited(help_city)) {
                prob_ptr[i] = 0.0;
            } else {
                prob_ptr[i] = Pheromone.getTotalNN(current.id, i, sub_idx);
                sum_prob += prob_ptr[i];
            }
        }

        if (sum_prob > 0.0) {
            double rnd = ran01(&seed);
            rnd *= sum_prob;
            int i = 0;
            double partial_sum = prob_ptr[i];
            prob_ptr[NN_ANTS] = HUGE_VAL;   /* sentinel: loop always stops */
            while (partial_sum <= rnd) {
                i++;
                partial_sum += prob_ptr[i];
            }
            if (i == NN_ANTS) {
                /* very rare rounding case (rnd close to 1) */
                neighbour_choose_best_next();
                return;
            }
            int help = knn[current.id][i];
            visit_component(componentList[help]);
            return;
        }

        // Tầng 2: backup knn
        static double backup_prob[MAX_KN_BACKUP + 1];
        double sum_backup = 0.0;
        for (int i = 0; i < NN_BACKUP_ANTS; ++i) {
            int help_city = knn_backup[current.id][i];
            if (memory.visited(help_city)) {
                backup_prob[i] = 0.0;
            } else {
                backup_prob[i] = Pheromone.getTotalArbitrary(current.id, help_city, sub_idx);
                sum_backup += backup_prob[i];
            }
        }

        if (sum_backup > 0.0) {
            double rnd = ran01(&seed);
            rnd *= sum_backup;
            int i = 0;
            double partial_sum = backup_prob[i];
            backup_prob[NN_BACKUP_ANTS] = HUGE_VAL;
            while (partial_sum <= rnd) {
                i++;
                partial_sum += backup_prob[i];
            }
            if (i == NN_BACKUP_ANTS) {
                neighbour_choose_best_next();
                return;
            }
            int help = knn_backup[current.id][i];
            visit_component(componentList[help]);
            return;
        }

        choose_best_next();
    }

    /*------------------------------------------------------------*/

    void online_delayed_pheromone_update() {

        for (int i = 0;
             i + 1 < state.sequenceSize;
             ++i)
        {
            Pheromone.deposit_pheromone_on_the_visited_arc(
                state.sequence[i],
                state.sequence[i + 1], sub_idx);
        }
    }

    void die() {}
};

