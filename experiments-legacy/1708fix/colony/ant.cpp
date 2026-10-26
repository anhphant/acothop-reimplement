extern long capacity;

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
    double adaptive_threshold;
    double adaptive_k;

    /*------------------------------------------------------------*/

    void initialize(size_t numberOfComponents) {
        state.sequence.clear();
        state.hypothetical_weight = 0.0;
        memory.reset(numberOfComponents);
    }

    bool termination_condition() const {
        return state.sequence.size() >= componentList.size();
    }

    /*------------------------------------------------------------*/

    void visit_component(const Component& component) {
        state.sequence.push_back(component);
        memory.visit(component.id);
    }

    /*------------------------------------------------------------*/

    long double transition_score(const Component& from, const Component& to) const {
        return Pheromone.getTotal(from.id, to.id);
    }

    /*------------------------------------------------------------*/

    struct Candidate {
        size_t index;
        double score;
    };

    vector<Candidate> build_candidates(const Component& current, double& totalScore) {
        vector<Candidate> candidates;
        totalScore = 0.0;

        bool only_dest_left = (state.sequence.size() == componentList.size() - 1);

        for (size_t i = 0; i < componentList.size(); ++i) {

            const Component& next = componentList[i];

            if (memory.visited(next.id))
                continue;

            if (next.id == componentList.back().id && !only_dest_left)
                continue;

            double score =
                transition_score(current, next);

            if (score <= 0.0 || !isfinite(score))
                continue;

            candidates.push_back({i, score});
            totalScore += score;
        }

        return candidates;
    }

    /*------------------------------------------------------------*/

    const Component* roulette_select(
        const vector<Candidate>& candidates,
        double totalScore)
    {
        if (candidates.empty())
            return nullptr;

        double r = rand_prob(rng);
        double cumulative = 0.0;

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

        double totalScore;
        const Component* selected = nullptr;

        auto candidates = 
            build_candidates(current, totalScore);

        double q0 = 0.5; // Balanced exploitation/exploration for 1000s run
        if (rand_prob(rng) < q0) {
            double best_score = -1.0;
            for (auto& c : candidates) {
                if (c.score > best_score) {
                    best_score = c.score;
                    selected = &componentList[c.index];
                }
            }
        } else {
            selected = roulette_select(candidates, totalScore);
        }

        //----------------------------------------------------------
        // Fallback
        //----------------------------------------------------------

        const Component* next = selected;

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