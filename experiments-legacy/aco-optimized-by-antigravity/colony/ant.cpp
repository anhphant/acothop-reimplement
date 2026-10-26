struct Ant {
    struct Memory {
        vector<bool> visited_components;
        vector<int> unvisited_nodes;
        vector<int> unvisited_pos;

        void reset(size_t n) {
            visited_components.assign(n, false);
            unvisited_nodes.resize(n);
            unvisited_pos.resize(n);
            for(size_t i=0; i<n; ++i) {
                unvisited_nodes[i] = i;
                unvisited_pos[i] = i;
            }
        }

        bool visited(size_t id) const {
            return visited_components[id];
        }

        void visit(size_t id) {
            visited_components[id] = true;
            int pos = unvisited_pos[id];
            int last_id = unvisited_nodes.back();
            unvisited_nodes[pos] = last_id;
            unvisited_pos[last_id] = pos;
            unvisited_nodes.pop_back();
        }
    };

    State state;
    Memory memory;
    bool is_elite = false;

    /*------------------------------------------------------------*/

    void initialize(size_t numberOfComponents) {
        state.sequence.clear();
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

    long double transition_score(const Component& from, const Component& to) const {
        const long double tau = Pheromone.getPheromone(from.id, to.id);
        const long double distance = connections.distance(from, to);

        if (distance <= 0.0L) return 0.0L;

        return pow(tau, params.alpha) * pow(1.0L / distance, params.beta);
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

        for (size_t k = 0; k < candidateList[current.id].size(); ++k) {
            int next_id = candidateList[current.id][k];

            const Component& next = componentList[next_id];

            if (memory.visited(next.id))
                continue;

            if (next.id == componentList.back().id && !only_dest_left)
                continue;

            long double tau = Pheromone.getPheromone(current.id, next.id);
            long double score = pow(tau, params.alpha) * candidateEta[current.id][k];

            if (score <= 0.0L || !isfinite(score))
                continue;

            candidates.push_back({(size_t)next_id, score});
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
        bool only_dest_left = (state.sequence.size() == componentList.size() - 1);
        int dest_id = componentList.back().id;

        for (int id : memory.unvisited_nodes) {
            if (id == dest_id && !only_dest_left) continue;
            return &componentList[id];
        }
        if (!memory.visited(dest_id)) return &componentList[dest_id];
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

            // log_debug(
            //     "aco: ant starts at ",
            //     componentList.front().id);

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