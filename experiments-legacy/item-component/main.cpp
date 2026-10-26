#include <bits/stdc++.h>
#include "helper/helper.cpp"
#include "problem/thopproblem.cpp"
#include "aco.cpp"
using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        log_debug("Usage: ", argv[0],
             " <input> --ants N --time-limit T --local-search N",
             " --alpha1 A --alpha2 B --alpha3 C --alpha4 D --epsilon E --rho R --delta D",
             " --seed S --output FILE [--log FILE] --ptries N",
             " --step-online BOOL --delayed-online BOOL");
        return 1;
    }

    Parameter p;
    p.inputfile = argv[1];

    bool ants = false, time = false, ls = false;
    bool alpha1 = false, alpha2 = false, alpha3 = false, alpha4 = false, epsilon = false, rho = false, delta = false;
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
        else if (a == "--epsilon")        p.epsilon = stold(v), epsilon = true;
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
    log_debug("main: seed = ", p.seed,
         ", output = ", p.outputfile,
         ", log = ", (log ? p.logfile : string("(disabled)")));
    log_debug("main: ptries = ", p.ptries);
    log_debug("main: step-online = ", p.flag_online_step_by_step_pheromone_update,
         ", delayed-online = ", p.flag_online_delayed_pheromone_update);

    if (!(ants && time && ls && alpha1 && alpha2 && alpha3 && alpha4 && epsilon && rho && delta &&
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


    ACOinit();
    log_debug("main: AntColonyOptimization initialized");

    start_time = chrono::steady_clock::now();

    int generation = 0;
    while(!termination_criterion()) {
        log_debug("aco: generation ", generation, " start");
        Pheromone.compute_total_information();
        Colony.ants_generation_and_activity();
        Pheromone.pheromone_evaporation();
        Optional.daemon_actions();
        ++generation;
        // 3 these activites needn't to be ordered
    }
    log_debug("aco: stopping after ", generation, " generations");

    cout << fixed << setprecision(10)
         << "Best objective: " << Optional.best_cost << '\n'
         << "Runtime: " << elapsed_time() << " s\n";

    if (originalCerrBuffer != nullptr) {
        cerr.rdbuf(originalCerrBuffer);
    }

    return 0;
}