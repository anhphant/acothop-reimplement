struct Ant {
    struct Memory {
        vector<bool> visited_components;

        void reset(size_t n) {
            visited_components.assign(n, false);
        }

        bool visited(size_t id) const {
            return visited_components[id];
        }

        void visit(size_t id) {
            visited_components[id] = true;
        }
    };

    State state;
    Memory memory;
    bool is_terminated = false;

    /*------------------------------------------------------------*/

    void initialize(size_t numberOfComponents) {
        state.restart();
        memory.reset(numberOfComponents);
        is_terminated = false;
    }

    bool termination_condition() const {
        return is_terminated || state.sequence.size() >= componentList.size();
    }

    /*------------------------------------------------------------*/

    void visit_component(const Component& component) {
        state.add_component(component);
        memory.visit(component.id);
    }

    long double transition_score(const Component& from, const Component& to) const {
        return Pheromone.getTotal(from.id, to.id);
    }

    /*------------------------------------------------------------*/

    struct Candidate {
        size_t index;
        long double score;
    };

    vector<Candidate> build_candidates(const Component& current, long double& totalScore) {
        vector<Candidate> candidates;
        totalScore = 0.0L;

        bool only_dest_left = (state.sequence.size() == componentList.size() - 1);

        for (size_t i = 0; i < componentList.size(); ++i) {

            const Component& next = componentList[i];

            if (memory.visited(next.id))
                continue;

            if (next.id == componentList.back().id && !only_dest_left)
                continue;

            if (!state.can_add(next))
                continue;

            long double score =
                transition_score(current, next);

            if (score <= 0.0L || !isfinite(score))
                continue;

            candidates.push_back({i, score});
            totalScore += score;
        }

        return candidates;
    }

    /*------------------------------------------------------------*/

    const Component* roulette_select(
        const vector<Candidate>& candidates,
        long double totalScore)
    {
        if (candidates.empty())
            return nullptr;

        long double r = rand_prob(rng);
        long double cumulative = 0.0L;

        for (auto& c : candidates) {

            cumulative += c.score / totalScore;

            if (r <= cumulative)
                return &componentList[c.index];
        }

        return &componentList[candidates.back().index];
    }

    /*------------------------------------------------------------*/

    const Component* first_unvisited() {
        for (auto& c : componentList) {
            if (!memory.visited(c.id)) {
                return &c;
            }
        }
        return nullptr;
    }

    /*------------------------------------------------------------*/

    void move_next() {

        if (componentList.empty())
            return;

        if (termination_condition())
            return;

        //----------------------------------------------------------
        // Start node
        //----------------------------------------------------------

        if (state.sequence.empty()) {
            visit_component(componentList.front());
            // log_debug("aco: ant starts at ", componentList.front().id);
            return;
        }

        //----------------------------------------------------------
        // Construct
        //----------------------------------------------------------

        const Component& current =
            state.sequence.back();

        long double totalScore;

        auto candidates =
            build_candidates(current, totalScore);

        const Component* next =
            roulette_select(candidates, totalScore);

        //----------------------------------------------------------
        // Fallback
        //----------------------------------------------------------

        if (next == nullptr) {
            for (auto& c : componentList) {
                if (!memory.visited(c.id) && state.can_add(c)) {
                    next = &c;
                    break;
                }
            }
        }

        if (next == nullptr) {
            is_terminated = true;
            return;
        }

        visit_component(*next);

        // log_debug(
        //     "aco: ant move ",
        //     current.id,
        //     " -> ",
        //     next->id);
    }

    /*------------------------------------------------------------*/

    void online_delayed_pheromone_update() {

        for (size_t i = 0;
             i + 1 < state.sequence.size();
             ++i)
        {
            Pheromone.deposit_pheromone_on_the_visited_arc(
                state.sequence[i].id,
                state.sequence[i + 1].id);
        }
    }

    void die() {}
};