class Optional {
public:
    State best_solution;
    long double best_cost = -INF;
    bool has_best_solution = false;

    void save_best_solution() {
        for (auto& ant : Colony.ants) {
            if (ant.state.sequence.empty() || ant.state.sequence.back().id != componentList.back().id)
                continue;

            long double cost = ant.state.solution_cost();

            if (!has_best_solution || cost > best_cost) {
                best_cost = cost;
                best_solution = ant.state;
                has_best_solution = true;
                log_debug("aco: new best solution found with cost ", cost,
                     " and ", best_solution.sequence.size(), " components");
            }
        }
    }

    void two_opt(State& s) {
        for (size_t i = 0; i + 2 < s.sequence.size(); i++)
            for (size_t j = i + 2; j + 1 < s.sequence.size(); j++)
                if (connections.distance(s.sequence[i], s.sequence[j]) +
                    connections.distance(s.sequence[i+1], s.sequence[j+1]) <
                    connections.distance(s.sequence[i], s.sequence[i+1]) +
                    connections.distance(s.sequence[j], s.sequence[j+1])) {
                    reverse(s.sequence.begin()+i+1, s.sequence.begin()+j+1);
                    return;
                }
    }

    void two_point_five_opt(State& s) {
        two_opt(s);
        for (size_t i = 0; i + 1 < s.sequence.size(); i++)
            for (size_t j = 1; j + 1 < s.sequence.size(); j++) {
                if (j == i || j == i+1) continue;
                State t = s;
                auto x = t.sequence[j];
                t.sequence.erase(t.sequence.begin()+j);
                t.sequence.insert(t.sequence.begin()+i+(j>i), x);
                if (t.solution_cost() < s.solution_cost()) {
                    s = t;
                    return;
                }
            }
    }

    void three_opt(State& s) {
        two_opt(s);

        for (size_t i = 0; i + 4 < s.sequence.size(); i++)
            for (size_t j = i + 2; j + 2 < s.sequence.size(); j++)
                for (size_t k = j + 2; k < s.sequence.size(); k++) {
                    State t=s;
                    reverse(t.sequence.begin()+i+1,t.sequence.begin()+j+1);
                    reverse(t.sequence.begin()+j+1,t.sequence.begin()+k);
                    if(t.solution_cost()<s.solution_cost()) {
                        s = t;
                        return;
                    }
                }
    }

    void local_search() {
        if (!has_best_solution) {
            return;
        }

        switch (params.local_search_flag) {
            case 0:
                // No local search
                break;

            case 1:
                // TODO: 2-opt
                two_opt(best_solution);
                break;

            case 2:
                // TODO: 2.5-opt
                two_point_five_opt(best_solution);
                break;

            case 3:
                // TODO: 3-opt
                three_opt(best_solution);
                break;

            default:
                break;
        }
    }

    void daemon_actions() {
        // 1. Lưu best solution từ generation hiện tại
        save_best_solution();

        // 2. Optional local search
        local_search();

        // 3. Nếu LS cải thiện solution thì cập nhật best_cost
        if (has_best_solution) {
            best_cost = best_solution.solution_cost();
        } else {

        }
    }

    void reset() {
        best_solution.restart();
        best_cost = -INF;
        has_best_solution = false;
    }
} Optional;