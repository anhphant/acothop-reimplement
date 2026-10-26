class Colony {

public:

    vector<Ant> ants;

    //------------------------------------------------------------

    void initialize_ants() {
        for (auto& ant : ants) {
            if (!ant.is_elite) {
                ant.initialize(componentList.size());
            } else {
                ant.is_elite = false; // Reset flag for next generation eval
            }
        }
    }

    //------------------------------------------------------------

    bool move_one_step(Ant& ant) {

        if (ant.termination_condition())
            return false;

        ant.move_next();

        if (params.flag_online_step_by_step_pheromone_update &&
            ant.state.sequence.size() >= 2)
        {
            auto& seq = ant.state.sequence;

            Pheromone.deposit_pheromone_on_the_visited_arc(
                seq[seq.size() - 2].id,
                seq.back().id);
        }

        return true;
    }

    //------------------------------------------------------------

    bool step() {

        bool moved = false;

        for (auto& ant : ants)
            moved |= move_one_step(ant);

        return moved;
    }

    //------------------------------------------------------------

    void online_delayed_update() {

        if (!params.flag_online_delayed_pheromone_update)
            return;

        for (auto& ant : ants)
            ant.online_delayed_pheromone_update();
    }

    //------------------------------------------------------------

    void ants_generation_and_activity() {

        initialize_ants();

        while (!termination_criterion()) {

            if (!step())
                break;
        }

        online_delayed_update();
    }

} Colony;