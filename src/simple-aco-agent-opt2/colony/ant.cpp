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

    /*------------------------------------------------------------*/

    void initialize(size_t numberOfComponents) {
        state.sequence.clear();
        memory.reset(numberOfComponents);
    }

    bool termination_condition() const {
        return !state.sequence.empty() && state.sequence.back().id == componentList.back().id;
    }

    /*------------------------------------------------------------*/

    void visit_component(const Component& component) {
        state.sequence.push_back(component);
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

        // --------------------------------------------------------
        // ESACO Algorithm 4 - Hit Domain:
        // Step 1: Thử các node trong nn_list (nearest neighbors)
        // --------------------------------------------------------
        for (int neighbor_id : nn_list[current.id]) {
            if (memory.visited(neighbor_id))
                continue;

            const Component& next = componentList[neighbor_id];
            long double score = transition_score(current, next);

            if (score <= 0.0L || !isfinite(score))
                continue;

            candidates.push_back({(size_t)neighbor_id, score});
            totalScore += score;
        }

        // --------------------------------------------------------
        // ESACO Algorithm 4 - Hit Domain:
        // Step 2: Thêm các node trong sparse pheromone list của node hiện tại
        // (các node đã từng được chọn từ đây → có khả năng cao được chọn lại)
        // --------------------------------------------------------
        if (candidates.empty()) {
            for (int neighbor_id : Pheromone.get_sparse_neighbors(current.id)) {
                if (memory.visited(neighbor_id))
                    continue;

                const Component& next = componentList[neighbor_id];
                long double score = transition_score(current, next);

                if (score <= 0.0L || !isfinite(score))
                    continue;

                candidates.push_back({(size_t)neighbor_id, score});
                totalScore += score;
            }
        }

        // --------------------------------------------------------
        // ESACO Algorithm 4 - Fault Domain:
        // Step 3: Fallback - thử toàn bộ node chưa thăm
        // --------------------------------------------------------
        if (candidates.empty()) {
            for (size_t i = 0; i < componentList.size(); ++i) {

                const Component& next = componentList[i];

                if (memory.visited(next.id))
                    continue;

                long double score = transition_score(current, next);

                if (score <= 0.0L || !isfinite(score))
                    continue;

                candidates.push_back({i, score});
                totalScore += score;
            }
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

        if (next == nullptr)
            next = first_unvisited();

        if (next == nullptr)
            return;

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