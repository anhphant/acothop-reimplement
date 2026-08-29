struct Ant {
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
        return Pheromone.getTotal(from.id, to.id);
    }

    /*------------------------------------------------------------*/

    // Faithful port of realbench choose_best_next (ants.c:302): pick the
    // unvisited city with maximal total[pheromone*heuristic].
    void choose_best_next() {
        const Component& current = componentList[state.sequence[state.sequenceSize - 1]];
        int next_city = nCities + 1;   // sentinel == instance.n
        double value_best = -1.0;      // total matrix values are always >= 0
        for (int city = 0; city < nCities; ++city) {
            if (memory.visited(city)) continue;
            double help = transition_score(current, componentList[city]);
            if (help > value_best) { value_best = help; next_city = city; }
        }
        visit_component(componentList[next_city]);
    }

    /*------------------------------------------------------------*/

    // Faithful port of realbench neighbour_choose_best_next (ants.c:335).
    void neighbour_choose_best_next() {
        const Component& current = componentList[state.sequence[state.sequenceSize - 1]];
        int next_city = nCities + 1;
        double value_best = -1.0;
        for (int i = 0; i < NN_ANTS; ++i) {
            int help_city = knn[current.id][i];
            if (memory.visited(help_city)) continue;
            double help = transition_score(current, componentList[help_city]);
            if (help > value_best) { value_best = help; next_city = help_city; }
        }
        if (next_city == nCities + 1)
            choose_best_next();
        else
            visit_component(componentList[next_city]);
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
                prob_ptr[i] = transition_score(current, componentList[help_city]);
                sum_prob += prob_ptr[i];
            }
        }

#if 0
        if (phase == 1) {
            cerr << "ANT_STEP1|sum_prob=" << fixed << setprecision(10) << sum_prob
                 << "|city0=" << knn[current.id][0]
                 << "|prob0=" << prob_ptr[0] << "\n";
        }
#endif

        if (sum_prob <= 0.0) {
            /* all cities in candidate set are tabu */
            choose_best_next();
            return;
        }

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
    }

    /*------------------------------------------------------------*/

    void online_delayed_pheromone_update() {

        for (int i = 0;
             i + 1 < state.sequenceSize;
             ++i)
        {
            Pheromone.deposit_pheromone_on_the_visited_arc(
                state.sequence[i],
                state.sequence[i + 1]);
        }
    }

    void die() {}
};

