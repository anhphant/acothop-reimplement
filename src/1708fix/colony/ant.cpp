extern long capacity;

struct Ant {
    struct Memory {
        bool visited_components[MAX_CITIES];

        void reset(size_t n) {
            for (size_t i = 0; i < n; i++) visited_components[i] = false;
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

    // FACO specific
    int next_node_in_source[MAX_CITIES];
    int new_edges_count = 0;
    int min_new_edges = 16;
    bool has_source = false;
    
    size_t candidates_index_arr[MAX_CITIES];
    double candidates_score_arr[MAX_CITIES];
    int candidates_len = 0;

    // Track perturbed nodes for Change-aware LS
    bool perturbed_nodes[MAX_CITIES];

    const State* src_state = nullptr;

    /*------------------------------------------------------------*/

    void initialize(size_t numberOfComponents, const State* source_state = nullptr, int min_new = 16) {
        state.sequence_len = 0;
        state.hypothetical_weight = 0.0;
        memory.reset(numberOfComponents);
        candidates_len = 0;
        new_edges_count = 0;
        min_new_edges = min_new;
        src_state = source_state;
        for (int i = 0; i < numComponents; i++) perturbed_nodes[i] = false;
        
        if (source_state != nullptr && source_state->sequence_len > 0) {
            has_source = true;
            for (int i = 0; i < numComponents; i++) next_node_in_source[i] = -1;
            for (int i = 0; i + 1 < source_state->sequence_len; i++) {
                next_node_in_source[source_state->sequence_arr[i]->id] = source_state->sequence_arr[i+1]->id;
            }
        } else {
            has_source = false;
        }
    }

    bool termination_condition() const {
        return state.sequence_len >= numComponents;
    }

    /*------------------------------------------------------------*/

    void visit_component(const Component& component) {
        state.sequence_arr[state.sequence_len++] = &component;
        memory.visit(component.id);
    }

    /*------------------------------------------------------------*/

    double transition_score(const Component& from, const Component& to) const {
        return Pheromone.getTotal(from.id, to.id);
    }

    /*------------------------------------------------------------*/

    void build_candidates(const Component& current, double& totalScore) {
        candidates_len = 0;
        totalScore = 0.0;

        bool only_dest_left = (state.sequence_len == numComponents - 1);
        int dest_id = componentList[numComponents - 1].id;

        for (int i = 0; i < numComponents; ++i) {
            if (i == current.id) continue;
            
            const Component& next = componentList[i];

            if (memory.visited(next.id))
                continue;

            if (next.id == dest_id && !only_dest_left)
                continue;

            double score = transition_score(current, next);

            if (score <= 0.0 || !isfinite(score))
                continue;

            candidates_index_arr[candidates_len] = i;
            candidates_score_arr[candidates_len] = score;
            candidates_len++;
            totalScore += score;
        }
    }

    /*------------------------------------------------------------*/

    const Component* roulette_select(double totalScore)
    {
        if (candidates_len == 0)
            return nullptr;

        double r = rand_prob(rng);
        double cumulative = 0.0;

        for (int i = 0; i < candidates_len; i++) {

            cumulative += candidates_score_arr[i] / totalScore;

            if (r <= cumulative)
                return &componentList[candidates_index_arr[i]];
        }

        return &componentList[candidates_index_arr[candidates_len - 1]];
    }

    /*------------------------------------------------------------*/

    const Component* first_unvisited() {
        double min_d = 1e9;
        const Component* best = nullptr;
        const Component& current = *state.sequence_arr[state.sequence_len - 1];
        bool only_dest_left = (state.sequence_len == numComponents - 1);
        int dest_id = componentList[numComponents - 1].id;

        for (size_t i = 0; i < numComponents; i++) {
            if (!memory.visited(componentList[i].id)) {
                if (componentList[i].id == dest_id && !only_dest_left) continue;
                double d = connections.distance(current, componentList[i]);
                if (d < min_d) {
                    min_d = d;
                    best = &componentList[i];
                }
            }
        }
        return best;
    }

    /*------------------------------------------------------------*/

    void move_next() {

        if (numComponents == 0)
            return;

        if (termination_condition())
            return;

        //----------------------------------------------------------
        // Start node
        //----------------------------------------------------------

        if (state.sequence_len == 0) {
            visit_component(componentList[0]);
            return;
        }

        //----------------------------------------------------------
        // Copy Mode (FACO Repair Mechanism)
        //----------------------------------------------------------
        
        if (has_source && new_edges_count >= min_new_edges) {
            // Traverse the source solution and copy remaining unvisited nodes
            for (int i = 0; i < src_state->sequence_len; ++i) {
                if (!memory.visited(src_state->sequence_arr[i]->id)) {
                    visit_component(*src_state->sequence_arr[i]);
                }
            }
            return;
        }

        //----------------------------------------------------------
        // Construct
        //----------------------------------------------------------

        const Component& current =
            *state.sequence_arr[state.sequence_len - 1];

        double totalScore;
        const Component* selected = nullptr;

        build_candidates(current, totalScore);

        double q0 = 0.5; // Balanced exploitation/exploration for 1000s run
        if (rand_prob(rng) < q0) {
            double best_score = -1.0;
            for (int i = 0; i < candidates_len; i++) {
                if (candidates_score_arr[i] > best_score) {
                    best_score = candidates_score_arr[i];
                    selected = &componentList[candidates_index_arr[i]];
                }
            }
        } else {
            selected = roulette_select(totalScore);
        }

        //----------------------------------------------------------
        // Fallback
        //----------------------------------------------------------

        const Component* next = selected;

        if (next == nullptr)
            next = first_unvisited();

        if (next == nullptr) {
            // absolute fallback if first_unvisited returned null due to dest_id constraint
            for (size_t i = 0; i < numComponents; i++) {
                if (!memory.visited(componentList[i].id)) {
                    next = &componentList[i];
                    break;
                }
            }
        }
        
        if (next == nullptr) return;

        if (has_source) {
            if (next_node_in_source[current.id] != next->id) {
                new_edges_count++;
                perturbed_nodes[current.id] = true;
                perturbed_nodes[next->id] = true;
            }
        }

        visit_component(*next);
    }

    /*------------------------------------------------------------*/

    void online_delayed_pheromone_update() {

        for (size_t i = 0;
             i + 1 < state.sequence_len;
             ++i)
        {
            Pheromone.deposit_pheromone_on_the_visited_arc(
                state.sequence_arr[i]->id,
                state.sequence_arr[i + 1]->id);
        }
    }

    void die() {}
};