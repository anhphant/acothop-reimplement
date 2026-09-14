extern const State* global_best_state;

class Colony {

public:

    Ant ants[1005]; // Max 1000 ants

    //------------------------------------------------------------

    void initialize_ants() {
        for (int i = 0; i < params.number_of_ants; i++) {
            const State* src = nullptr;
            if (global_best_state != nullptr && rand_prob(rng) < 0.8) {
                src = global_best_state;
            }
            ants[i].initialize(numComponents, src, 16);
        }
    }

    //------------------------------------------------------------

    bool move_one_step(Ant& ant) {

        if (ant.termination_condition())
            return false;

        ant.move_next();

        if (params.flag_online_step_by_step_pheromone_update &&
            ant.state.sequence_len >= 2)
        {
            auto& seq = ant.state.sequence_arr;
            int len = ant.state.sequence_len;

            Pheromone.deposit_pheromone_on_the_visited_arc(
                seq[len - 2]->id,
                seq[len - 1]->id);
        }

        return true;
    }

    //------------------------------------------------------------

    bool step() {

        bool moved = false;

        for (int i = 0; i < params.number_of_ants; i++)
            moved |= move_one_step(ants[i]);

        return moved;
    }

    //------------------------------------------------------------

    void online_delayed_update() {

        if (!params.flag_online_delayed_pheromone_update)
            return;

        for (int i = 0; i < params.number_of_ants; i++)
            ants[i].online_delayed_pheromone_update();
    }

    void adaptive_pheromone_evaporation() {
        // Find min/max values in pheromone matrix
        double max_tau = 0.0;
        double min_tau = 1.0;
        
        for (size_t i = 0; i < numComponents; ++i) {
            for (size_t j = 0; j < numComponents; ++j) {
                if (i != j) {
                    double tau = Pheromone.tau(componentList[i].id, componentList[j].id);
                    if (tau > max_tau) max_tau = tau;
                    if (tau < min_tau) min_tau = tau;
                }
            }
        }
        
        // Calculate spread and adjust rho
        double spread = max_tau / max(min_tau, 1e-10);
        double target_spread = 1000.0;
        
        if (spread > target_spread) {
            params.rho = min(0.99, params.rho * 1.05); // Increase evaporation to reduce spread
        } else if (spread < target_spread / 10) {
            params.rho = max(0.01, params.rho * 0.95); // Decrease evaporation to allow spread
        }
        
        Pheromone.evaporate(params.rho);
    }

    //------------------------------------------------------------

    void ants_generation_and_activity() {
        initialize_ants();

        if (params.flag_online_step_by_step_pheromone_update) {
            while (true) {
                if (!step()) break;
            }
        } else {
            for (int i = 0; i < params.number_of_ants; i++) {
                while (!ants[i].termination_condition()) {
                    ants[i].move_next();
                }
            }
        }

        online_delayed_update();
    }

} Colony;