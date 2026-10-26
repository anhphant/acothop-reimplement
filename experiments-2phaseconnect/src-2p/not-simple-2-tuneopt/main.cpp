#include <bits/stdc++.h>
#include "helper/helper.cpp"
#include "problem/thopproblem.cpp"
#include "packing/packing.cpp"
#include "aco.cpp"
using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        log_debug("Usage: ", argv[0],
             " <input> [--ants N] [--time-limit T] [--local-search N]",
             " [--alpha A] [--beta B] [--rho R] [--delta D]",
             " [--seed S] [--output FILE] [--log FILE] [--ptries N]",
             " [--nnants N] [--nnls N]");
        return 1;
    }

    Parameter p;
    p.number_of_ants = 500;
    p.time_limit = 100.0;
    p.local_search_flag = 1;
    p.alpha1 = 1.0;
    p.alpha2 = 1.0;
    p.alpha3 = 0.0;
    p.alpha4 = 0.0;
    p.eps1 = 0.1;
    p.eps2 = 0.1;
    p.rho = 0.1;
    p.delta = 1.0;
    p.seed = 1910;
    p.ptries = 1;
    p.nn_ants = 20;
    p.nn_ls = 20;

    int i = 1;
    if (argc > 1 && argv[1][0] != '-') {
        p.inputfile = argv[1];
        i = 2;
    }

    for (; i < argc; i++) {
        string a = argv[i];
        string v = "";
        if (i + 1 < argc && argv[i + 1][0] != '-') {
            v = argv[++i];
        }
        if      (a == "--inputfile")                             p.inputfile = v;
        else if (a == "--output" || a == "--outputfile")         p.outputfile = v;
        else if (a == "--log" || a == "--logfile")               p.logfile = v;
        else if (a == "--ants")                                  p.number_of_ants = stoi(v);
        else if (a == "--time" || a == "--time-limit")           p.time_limit = stold(v);
        else if (a == "--localsearch" || a == "--local-search")  p.local_search_flag = stoi(v);
        else if (a == "--alpha" || a == "--alpha1")              p.alpha1 = stold(v);
        else if (a == "--beta" || a == "--alpha2")               p.alpha2 = stold(v);
        else if (a == "--alpha3")                                p.alpha3 = stold(v);
        else if (a == "--alpha4")                                p.alpha4 = stold(v);
        else if (a == "--eps1")                                  p.eps1 = stold(v);
        else if (a == "--eps2")                                  p.eps2 = stold(v);
        else if (a == "--rho")                                   p.rho = stold(v);
        else if (a == "--rho-item")                              p.rho_item = stold(v);
        else if (a == "--alpha-item")                            p.alpha_item = stold(v);
        else if (a == "--delta")                                 p.delta = stold(v);
        else if (a == "--seed")                                  p.seed = stoi(v);
        else if (a == "--ptries")                                p.ptries = stoi(v);
        else if (a == "--nnants")                                p.nn_ants = stoi(v);
        else if (a == "--nnls")                                  p.nn_ls = stoi(v);
        else if (a == "--step-online")                           p.flag_online_step_by_step_pheromone_update = stoi(v);
        else if (a == "--delayed-online")                        p.flag_online_delayed_pheromone_update = stoi(v);
        else if (a == "--tries" || a == "--tours" || a == "--optimum" ||
                 a == "--q0" || a == "--elitistants" || a == "--rasranks" ||
                 a == "--dlb" || a == "--mmas" || a == "--as" || a == "--eas" ||
                 a == "--ras" || a == "--bwas" || a == "--acs") {
            // benign flags from aco++, ignore
        } else {
            log_debug("Unknown option: ", a);
            return 1;
        }
    }

    if (p.inputfile.empty()) {
        log_debug("main: error: input file not specified.");
        return 1;
    }
    if (p.ptries < 1) {
        log_debug("main: error: ptries must be >= 1.");
        return 1;
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
    log_debug("main: rho_item = ", p.rho_item, ", alpha_item = ", p.alpha_item);
    log_debug("main: seed = ", p.seed,
         ", output = ", p.outputfile,
         ", nnants = ", p.nn_ants, ", nnls = ", p.nn_ls);

    params = p;
    packParams.ptries = params.ptries;

    if (!p.logfile.empty()) {
        if (freopen(p.logfile.c_str(), "w", stderr) == NULL) {
            log_debug("Cannot create/open log file: ", p.logfile);
            return 1;
        }
        log_debug("Log started");
    }

    string report_filename = "";
    if (!p.outputfile.empty()) {
        report_filename = p.outputfile + ".log";
    }
    if (!report_filename.empty()) {
        log_report_file = fopen(report_filename.c_str(), "w");
        if (log_report_file) {
            fprintf(log_report_file, "Parameter-settings: \n\n");
            fprintf(log_report_file, "--inputfile          %s\n", p.inputfile.c_str());
            fprintf(log_report_file, "--outputfile         %s\n", p.outputfile.c_str());
            fprintf(log_report_file, "--tries              1\n");
            fprintf(log_report_file, "--tours              0\n");
            fprintf(log_report_file, "--ptries             %d\n", p.ptries);
            fprintf(log_report_file, "--time               %.2f\n", p.time_limit);
            fprintf(log_report_file, "--seed               %d\n", p.seed);
            fprintf(log_report_file, "--optimum            1\n");
            fprintf(log_report_file, "--ants               %d\n", p.number_of_ants);
            fprintf(log_report_file, "--nnants             %d\n", p.nn_ants);
            fprintf(log_report_file, "--alpha              %.2f\n", p.alpha1);
            fprintf(log_report_file, "--beta               %.2f\n", p.alpha2);
            fprintf(log_report_file, "--rho                %.2f\n", p.rho);
            fprintf(log_report_file, "--q0                 0.00\n");
            fprintf(log_report_file, "--elitistants        0\n");
            fprintf(log_report_file, "--rasranks           0\n");
            fprintf(log_report_file, "--localsearch        %d\n", p.local_search_flag);
            fprintf(log_report_file, "--nnls               %d\n", p.nn_ls);
            fprintf(log_report_file, "--dlb                1\n");
            fprintf(log_report_file, "--as                 0\n");
            fprintf(log_report_file, "--eas                0\n");
            fprintf(log_report_file, "--ras                0\n");
            fprintf(log_report_file, "--mmas               1\n");
            fprintf(log_report_file, "--bwas               0\n");
            fprintf(log_report_file, "--acs                0\n\n\n");
            fprintf(log_report_file, "begin try 0 \n");
            fflush(log_report_file);
        }
    }

    if (!readData(p.inputfile)) {
        log_debug("Cannot read: ", p.inputfile);
        return 1;
    }

    log_debug("main: input loaded (", nCities, " cities, ", nItems, " items)");

    packInit();
    log_debug("main: PackingPlan initialized");
    ACOinit();
    log_debug("main: AntColonyOptimization initialized");

    // Precompute top-8 p/w ratio aggregation per city (used by heuristic).
    compute_city_topk(8);
    log_debug("main: top-K city aggregation computed (K=8)");

    // Precompute static heuristic (distance + item) factors — done once, reused each iteration.
    Pheromone.compute_heuristic();
    log_debug("main: heuristic precomputed");

    // Initialize item pheromone with MMAS trail_0 based on rho_item and NN tour fitness.
    double fit = Optional.fitness(Optional.best_cost);
    if (fit <= 0) fit = 1.0;
    double item_trail_0 = 1.0 / (params.rho_item * fit);
    Pheromone.init_item_pheromone(item_trail_0);
    log_debug("main: item pheromone initialized (alpha_item=", params.alpha_item, ", rho_item=", params.rho_item, ", trail_0=", item_trail_0, ")");

    start_time = chrono::steady_clock::now();

    // Initial total information (matches realbench init_try: compute_total_information).
    Pheromone.compute_total_information();

    cout << "begin try 0 \n";
    cout.flush();

    int iteration = 1;   // matches realbench init_try (iteration starts at 1)
    while(!termination_criterion()) {
        log_debug("aco: generation ", iteration, " start");
        Colony.ants_generation_and_activity();
        Optional.update_statistics(iteration);
        Pheromone.pheromone_evaporation();
        Optional.daemon_actions(iteration);
        Optional.search_control_and_statistics(iteration);
        // Compute total for the next generation (matches realbench pheromone_trail_update).
        if (params.local_search_flag > 0)
            Pheromone.compute_nn_list_total_information();
        else
            Pheromone.compute_total_information();
        iteration++;
    }
    log_debug("aco: stopping after ", iteration, " generations");

    cout << "end try 0 \n\n";
    printf("try          0,        best %10ld,        found at iteration %10d,        found at time %10.2f\n\n",
           (long int)Optional.best_cost, Optional.restart_found_best, elapsed_time());
    cout.flush();

    if (log_report_file) {
        fprintf(log_report_file, "end try 0 \n\n\n");
        fprintf(log_report_file, "try          0,        best %10ld,        found at iteration %10d,        found at time %10.2f\n\n",
                (long int)Optional.best_cost, Optional.restart_found_best, elapsed_time());
        fflush(log_report_file);
        fclose(log_report_file);
        log_report_file = nullptr;
    }

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

