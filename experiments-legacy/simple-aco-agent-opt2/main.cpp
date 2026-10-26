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
             " --alpha A --beta B --rho R --delta D",
             " --seed S --output FILE [--log FILE] --ptries N",
             " --step-online BOOL --delayed-online BOOL");
        return 1;
    }

    Parameter p;
    p.inputfile = argv[1];

    bool ants = false, time = false, ls = false;
    bool alpha = false, beta = false, rho = false, delta = false;
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
        else if (a == "--alpha")          p.alpha = stold(v), alpha = true;
        else if (a == "--beta")           p.beta = stold(v), beta = true;
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
    log_debug("main: alpha = ", p.alpha, ", beta = ", p.beta,
         ", rho = ", p.rho, ", delta = ", p.delta);
    log_debug("main: seed = ", p.seed,
         ", output = ", p.outputfile,
         ", log = ", (log ? p.logfile : string("(disabled)")));
    log_debug("main: ptries = ", p.ptries);
    log_debug("main: step-online = ", p.flag_online_step_by_step_pheromone_update,
         ", delayed-online = ", p.flag_online_delayed_pheromone_update);

    if (!(ants && time && ls && alpha && beta && rho && delta &&
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

    ofstream logStream;
    streambuf* originalCerrBuffer = nullptr;
    if (log) {
        logStream.open(p.logfile, ios::out | ios::trunc);
        if (!logStream) {
            log_debug("Cannot create/open log file: ", p.logfile);
            return 1;
        }
        originalCerrBuffer = cerr.rdbuf();
        cerr.rdbuf(logStream.rdbuf());
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

    start_time = chrono::steady_clock::now();

    int generation = 0;
    while(!termination_criterion()) {
        auto t0 = chrono::steady_clock::now();
        Pheromone.compute_total_information();
        auto t1 = chrono::steady_clock::now();
        Colony.ants_generation_and_activity();
        auto t2 = chrono::steady_clock::now();
        Pheromone.pheromone_evaporation();
        auto t3 = chrono::steady_clock::now();
        Optional.daemon_actions(generation);
        auto t4 = chrono::steady_clock::now();
        
        double ms_total = chrono::duration<double, milli>(t1 - t0).count();
        double ms_ants = chrono::duration<double, milli>(t2 - t1).count();
        double ms_evap = chrono::duration<double, milli>(t3 - t2).count();
        double ms_daemon = chrono::duration<double, milli>(t4 - t3).count();
        
        log_debug("aco: generation ", generation, " timings: total_info=", ms_total, "ms, ants=", ms_ants, "ms, evap=", ms_evap, "ms, daemon=", ms_daemon, "ms");
        ++generation;
        // 3 these activites needn't to be ordered
    }
    log_debug("aco: stopping after ", generation, " generations");

    cout << fixed << setprecision(10)
         << "Best objective: " << Optional.best_cost << '\n'
         << "Runtime: " << elapsed_time() << " s\n";

    if (p.outputfile != "") {
        ofstream out(p.outputfile);
        if (out) {
            vector<int> best_tour;
            for (auto c : Optional.best_solution.sequence) {
                best_tour.push_back(c.id);
            }
            Packing best_pack = Optional.best_solution.best_packing;
            
            vector<int> visited;
            for (int city : best_tour) {
                if (best_pack.weight_at_city[city] > 0) {
                    visited.push_back(city);
                }
            }
            
            for (size_t i = 0; i < visited.size(); i++) {
                if (visited[i] == 0 || visited[i] == nCities - 1) continue;
                out << visited[i] + 1 << " ";
            }
            out << "\n";

            for (int i = 0; i < nItems; i++) {
                if (best_pack.picked[i]) {
                    out << i + 1 << " ";
                }
            }
            out << "\n";
            log_debug("main: saved best solution to ", p.outputfile);
        }
    }
    if (originalCerrBuffer != nullptr) {
        cerr.rdbuf(originalCerrBuffer);
    }

    return 0;
}