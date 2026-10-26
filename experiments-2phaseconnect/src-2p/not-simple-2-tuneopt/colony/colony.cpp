class Colony {

public:

    Ant ants[MAX_ANTS];
    int antsCount = 0;

    //------------------------------------------------------------

    void initialize_ants() {

        for (int a = 0; a < antsCount; ++a)
            ants[a].initialize(nCities);
    }

    //------------------------------------------------------------

    bool move_one_step(Ant& ant) {

        if (ant.termination_condition())
            return false;

        ant.move_next();

        if (params.flag_online_step_by_step_pheromone_update &&
            ant.state.sequenceSize >= 2)
        {
            int* seq = ant.state.sequence;

            Pheromone.deposit_pheromone_on_the_visited_arc(
                seq[ant.state.sequenceSize - 2],
                seq[ant.state.sequenceSize - 1]);
        }

        return true;
    }

    //------------------------------------------------------------

    bool step() {

        bool moved = false;

        for (int a = 0; a < antsCount; ++a)
            moved |= move_one_step(ants[a]);

        return moved;
    }

    //------------------------------------------------------------

    void online_delayed_update() {

        if (!params.flag_online_delayed_pheromone_update)
            return;

        for (int a = 0; a < antsCount; ++a)
            ants[a].online_delayed_pheromone_update();
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
