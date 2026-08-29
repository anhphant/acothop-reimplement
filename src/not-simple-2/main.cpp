#include <bits/stdc++.h>
#include "helper/helper.cpp"
#include "problem/thopproblem.cpp"
#include "packing/packing.cpp"
#include "aco.cpp"
using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        log_debug("Usage: ", argv[0],
             " <input> --ants N --time-limit T --local-search N",
             " --alpha1 A1 --alpha2 A2 --alpha3 A3 --alpha4 A4",
             " --eps1 E1 --eps2 E2 --rho R --delta D",
             " --seed S --output FILE [--log FILE] --ptries N",
             " --step-online BOOL --delayed-online BOOL");
        return 1;
    }

    Parameter p;
    p.inputfile = argv[1];

    bool ants = false, time = false, ls = false;
    bool alpha1 = false, alpha2 = false, alpha3 = false, alpha4 = false;
    bool eps1 = false, eps2 = false;
    bool rho = false, delta = false;
    bool seed = false, output = false, log = false;
    bool step = false, delayed = false;

    for (int i = 2; i < argc; i += 2) {
        if (i + 1 >= argc) {
            log_debug("Missing value for ", argv[i]);
            return 1;
        }

        string a = argv[i];
        string v = argv[i + 1];

        if      (a == "--ants")           p.number_of_ants = stoi(v), ants = true;
        else if (a == "--time-limit")     p.time_limit = stold(v), time = true;
        else if (a == "--local-search")   p.local_search_flag = stoi(v), ls = true;
        else if (a == "--alpha1")         p.alpha1 = stold(v), alpha1 = true;
        else if (a == "--alpha2")         p.alpha2 = stold(v), alpha2 = true;
        else if (a == "--alpha3")         p.alpha3 = stold(v), alpha3 = true;
        else if (a == "--alpha4")         p.alpha4 = stold(v), alpha4 = true;
        else if (a == "--eps1")           p.eps1 = stold(v), eps1 = true;
        else if (a == "--eps2")           p.eps2 = stold(v), eps2 = true;
        else if (a == "--rho")            p.rho = stold(v), rho = true;
        else if (a == "--delta")          p.delta = stold(v), delta = true;
        else if (a == "--seed")           p.seed = stoi(v), seed = true;
        else if (a == "--output")         p.outputfile = v, output = true;
        else if (a == "--log")            p.logfile = v, log = true;
        else if (a == "--ptries")         p.ptries = stoi(v);
        else if (a == "--step-online")    p.flag_online_step_by_step_pheromone_update = stoi(v), step = true;
        else if (a == "--delayed-online") p.flag_online_delayed_pheromone_update = stoi(v), delayed = true;
        else {
            log_debug("Unknown option: ", a);
            return 1;
        }
    }

    log_debug("main: parsed arguments");
    log_debug("main: input file = ", p.inputfile);
    log_debug("main: ants = ", p.number_of_ants,
         ", time-limit = ", p.time_limit,
         ", local-search = ", p.local_search_flag);
    log_debug("main: alpha1 = ", p.alpha1, ", alpha2 = ", p.alpha2,
         ", alpha3 = ", p.alpha3, ", alpha4 = ", p.alpha4);
    log_debug("main: eps1 = ", p.eps1, ", eps2 = ", p.eps2,
         ", rho = ", p.rho, ", delta = ", p.delta);
    log_debug("main: seed = ", p.seed,
         ", output = ", p.outputfile,
         ", log = ", (log ? p.logfile : string("(disabled)")));
    log_debug("main: ptries = ", p.ptries);
    log_debug("main: step-online = ", p.flag_online_step_by_step_pheromone_update,
         ", delayed-online = ", p.flag_online_delayed_pheromone_update);

    if (!(ants && time && ls && alpha1 && alpha2 && alpha3 && alpha4 &&
          eps1 && eps2 && rho && delta &&
          seed && output && step && delayed)) {
        log_debug("main: error: all required parameters are missing or incomplete.");
        return 1;
    }

    if (p.ptries < 1) {
        log_debug("main: error: ptries must be >= 1.");
        return 1;
    }

    log_debug("main: parameter validation passed; preparing solver initialization");

    params = p;
    packParams.ptries = params.ptries;

    if (log) {
        // Redirect C stderr into the log file so BOTH log_debug (cerr) and
        // fprintf(stderr, ...) (e.g. PACKLOG in packing.cpp) land in --log FILE.
        if (freopen(p.logfile.c_str(), "w", stderr) == NULL) {
            log_debug("Cannot create/open log file: ", p.logfile);
            return 1;
        }
        log_debug("Log started");
    } else {
        log_debug("main: logging disabled");
    }

    log_debug("main: parameter validation passed");

    if (!readData(p.inputfile)) {
        log_debug("Cannot read: ", p.inputfile);
        return 1;
    }

    log_debug("main: input loaded (", nCities, " cities, ", nItems, " items)");

    packInit();
    log_debug("main: PackingPlan initialized");
    ACOinit();
    log_debug("main: AntColonyOptimization initialized");

    // Precompute static heuristic (distance + item) factors — done once, reused each iteration.
    Pheromone.compute_heuristic();
    log_debug("main: heuristic precomputed");

    start_time = chrono::steady_clock::now();

    // Initial total information (matches realbench init_try: compute_total_information).
    Pheromone.compute_total_information();

    int iteration = 1;   // matches realbench init_try (iteration starts at 1)
    while(!termination_criterion()) {
        log_debug("aco: generation ", iteration, " start");
                auto t1 = chrono::high_resolution_clock::now();
        Colony.ants_generation_and_activity();
        auto t2 = chrono::high_resolution_clock::now();
        Optional.update_statistics(iteration);
        Pheromone.pheromone_evaporation();
        auto t3 = chrono::high_resolution_clock::now();
        Optional.daemon_actions(iteration);
        auto t4 = chrono::high_resolution_clock::now();
        cout << "Colony: " << chrono::duration_cast<chrono::milliseconds>(t2 - t1).count() << "ms" << endl;
        cout << "Update: " << chrono::duration_cast<chrono::milliseconds>(t3 - t2).count() << "ms" << endl;
        cout << "Daemon: " << chrono::duration_cast<chrono::milliseconds>(t4 - t3).count() << "ms" << endl;
        Optional.search_control_and_statistics(iteration);
        // Compute total for the next generation (matches realbench pheromone_trail_update).
        if (params.local_search_flag > 0)
            Pheromone.compute_nn_list_total_information();
        else
            Pheromone.compute_total_information();
        ++iteration;
        // 3 these activites needn't to be ordered
    }
    log_debug("aco: stopping after ", iteration, " generations");

    cout << fixed << setprecision(10)
         << "Best objective: " << Optional.best_cost << '\n'
         << "Runtime: " << elapsed_time() << " s\n";

    if (!p.outputfile.empty()) {
        ofstream out(p.outputfile);
        if (out.is_open()) {
            int tour[MAX_CITIES];
            int tourLen = Optional.best_solution.sequenceSize;
            for (int i = 0; i < tourLen; ++i) tour[i] = Optional.best_solution.sequence[i];
            Packing ultimate_packing = Optional.best_solution.packing;

            int output_cities[MAX_CITIES];
            int outCount = 0;
            for (int i = 1; i + 1 < tourLen; ++i) {
                int city = tour[i];
                bool hasPicked = false;
                for (int idx = cityItemStart[city]; idx < cityItemStart[city + 1]; ++idx) {
                    if (ultimate_packing.picked[itemsByCity[idx]]) {
                        hasPicked = true;
                        break;
                    }
                }
                if (hasPicked) {
                    output_cities[outCount++] = city;
                }
            }

            out << "[";
            for (int i = 0; i < outCount; ++i) {
                out << (output_cities[i] + 1);
                if (i + 1 < outCount) out << ", ";
            }
            out << "]\n";

            // items: list ALL picked items (checker reconstructs tour[1..n] and
            // sums collected items; dropping start/destination items under-reports)
            out << "[";
            bool first = true;
            for (int j = 0; j < nItems; ++j) {
                if (ultimate_packing.picked[j]) {
                    if (!first) out << ", ";
                    out << (j + 1);
                    first = false;
                }
            }
            out << "]\n";
            out.close();
            log_debug("main: saved best solution to ", p.outputfile);
        } else {
            log_debug("main: failed to open output file: ", p.outputfile);
        }
    }

    return 0;
}

