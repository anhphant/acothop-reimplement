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
             " --alpha1 A1 --alpha2 A2 --alpha3 A3 --alpha4 A4 --rho R --delta D",
             " --seed S --output FILE [--log FILE] --ptries N",
             " --step-online BOOL --delayed-online BOOL");
        return 1;
    }

    Parameter p;
    p.inputfile = argv[1];

    bool ants = false, time = false, ls = false;
    bool alpha1 = false, alpha2 = false, alpha3 = false, alpha4 = false, rho = false, delta = false;
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
        else if (a == "--alpha1")          p.alpha1 = stold(v), alpha1 = true;
        else if (a == "--alpha2")          p.alpha2 = stold(v), alpha2 = true;
        else if (a == "--alpha3")          p.alpha3 = stold(v), alpha3 = true;
        else if (a == "--alpha4")          p.alpha4 = stold(v), alpha4 = true;
        else if (a == "--rho")            p.rho = stold(v), rho = true;
        else if (a == "--delta")          p.delta = stold(v), delta = true;
        else if (a == "--seed")           p.seed = stoi(v), seed = true;
        else if (a == "--output")         p.outputfile = v, output = true;
        else if (a == "--log")            p.logfile = v, log = true;
        else if (a == "--ptries")         p.ptries = stoi(v);
        else if (a == "--step-online")    p.flag_online_step_by_step_pheromone_update = stoi(v), step = true;
        else if (a == "--delayed-online") p.flag_online_delayed_pheromone_update = stoi(v), delayed = true;
        else if (a == "--adaptive")       p.adaptive_heuristic = (stoi(v) != 0);
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

    if (!(ants && time && ls && alpha1 && alpha2 && alpha3 && alpha4 && rho && delta &&
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
    packParams.ptries = 1;

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
        log_debug("aco: generation ", generation, " start, time: ", elapsed_time());
        Pheromone.compute_total_information();
        log_debug("aco: compute_total_information done, time: ", elapsed_time());
        Colony.ants_generation_and_activity();
        log_debug("aco: ants_generation_and_activity done, time: ", elapsed_time());
        Pheromone.adaptive_pheromone_evaporation(Colony.ants, 0.05L, (params.rho > 0.05L ? params.rho : 0.63L));
        log_debug("aco: adaptive_pheromone_evaporation done, time: ", elapsed_time());
        Optional.daemon_actions();
        log_debug("aco: daemon_actions done, time: ", elapsed_time());
        ++generation;
        // 3 these activites needn't to be ordered
    }
    log_debug("aco: stopping after ", generation, " generations");

    // =======================================================================
    // ULTIMATE TOUR OPTIMIZATION (THOP OBJECTIVE)
    // =======================================================================
    vector<int> best_tour;
    for (auto c : Optional.best_solution.sequence) best_tour.push_back(c.id);
    
    int old_ptries = packParams.ptries;
    packParams.ptries = p.ptries;
    
    // Use the fast stochastic EA packing algorithm!
    log_debug("main: starting Ultimate Tour Optimization");
    double uto_start = elapsed_time();
    Packing ultimate_packing = pack(best_tour, 1000);
    double ultimate_profit = computeFitness(best_tour, ultimate_packing);
    
    bool global_improved = true;
    while (global_improved && elapsed_time() < params.time_limit) {
        log_debug("uto: new global improvement loop, time: ", elapsed_time());
        global_improved = false;
        
        // 1. SUBTOUR 2-OPT OPTIMIZATION
        {
            vector<int> visited;
            vector<int> unvisited;
            for (int city : best_tour) {
                if (city == best_tour.front() || city == best_tour.back()) continue;
                bool has = false;
                for (int item : itemsAtCity[city]) {
                    if (ultimate_packing.picked[item]) { has = true; break; }
                }
                if (has) visited.push_back(city);
                else unvisited.push_back(city);
            }
            
            vector<int> subpath;
            subpath.push_back(best_tour.front());
            for (int c : visited) subpath.push_back(c);
            subpath.push_back(best_tour.back());
            
            bool sub_improved = true;
            while (sub_improved && elapsed_time() < params.time_limit) {
                sub_improved = false;
                for (size_t i = 1; i + 2 < subpath.size(); ++i) {
                    for (size_t j = i + 1; j + 1 < subpath.size(); ++j) {
                        int u = subpath[i-1], v = subpath[i];
                        int x = subpath[j], y = subpath[j+1];
                        long double cur_d = connections.distance(componentList[u], componentList[v]) +
                                            connections.distance(componentList[x], componentList[y]);
                        long double new_d = connections.distance(componentList[u], componentList[x]) +
                                            connections.distance(componentList[v], componentList[y]);
                        if (new_d < cur_d - 1e-6) {
                            reverse(subpath.begin() + i, subpath.begin() + j + 1);
                            sub_improved = true;
                            break;
                        }
                    }
                    if (sub_improved) break;
                }
            }
            
            vector<int> new_tour = subpath;
            new_tour.pop_back();
            for (int u : unvisited) new_tour.push_back(u);
            new_tour.push_back(best_tour.back());
            
            double f = computeFitness(new_tour, ultimate_packing);
            if (f >= ultimate_profit) {
                best_tour = new_tour;
                ultimate_profit = f;
            }
        }

        // 2. ULTIMATE TOUR 2-OPT OPTIMIZATION (THOP FITNESS)
        bool tour_improved = true;
        while (tour_improved && elapsed_time() < params.time_limit) {
            tour_improved = false;
            for (size_t i = 1; i + 2 < best_tour.size(); ++i) {
                for (size_t j = i + 1; j + 1 < best_tour.size(); ++j) {
                    if (elapsed_time() >= params.time_limit) break;
                    
                    vector<int> neighbor_tour = best_tour;
                    reverse(neighbor_tour.begin() + i, neighbor_tour.begin() + j + 1);
                    
                    double f = computeFitness(neighbor_tour, ultimate_packing);
                    
                    if (f > ultimate_profit) {
                        ultimate_profit = f;
                        best_tour = neighbor_tour;
                        tour_improved = true;
                        global_improved = true;
                        break;
                    }
                }
            }
        }
        
        // 3. REVERSE CHECK
        vector<int> rev_tour = best_tour;
        reverse(rev_tour.begin() + 1, rev_tour.end() - 1);
        double rev_profit = computeFitness(rev_tour, ultimate_packing);
        if (rev_profit > ultimate_profit) {
            ultimate_profit = rev_profit;
            best_tour = rev_tour;
            global_improved = true;
        }
        
        // 4. ULTIMATE PACKING OPTIMIZATION
        Packing new_packing = pack(best_tour, 100);
        double f = computeFitness(best_tour, new_packing);
        if (f > ultimate_profit) {
            ultimate_profit = f;
            ultimate_packing = new_packing;
            global_improved = true;
        }
    }
    
    packParams.ptries = old_ptries;
    if (ultimate_profit > Optional.best_cost) {
        Optional.best_cost = ultimate_profit;
    }
    // =======================================================================

    // Compute pure TSP distance of best_tour
    long double pure_tsp_distance = 0;
    for (size_t i = 0; i + 1 < best_tour.size(); ++i) {
        pure_tsp_distance += connections.distance(componentList[best_tour[i]], componentList[best_tour[i+1]]);
    }
    cout << fixed << setprecision(10)
         << "Best objective: " << Optional.best_cost << '\n'
         << "Pure TSP distance of best tour: " << pure_tsp_distance << '\n';

    if (output && !p.outputfile.empty()) {
        ofstream out(p.outputfile);
        if (out) {
            // Reconstruct visited cities (the ones where items were picked)
            vector<int> output_cities;
            for (size_t i = 1; i + 1 < best_tour.size(); ++i) {
                int city = best_tour[i];
                bool hasPicked = false;
                for (int item : itemsAtCity[city]) {
                    if (ultimate_packing.picked[item]) {
                        hasPicked = true;
                        break;
                    }
                }
                if (hasPicked) {
                    output_cities.push_back(city);
                }
            }

            // Write middle cities (1-indexed)
            for (size_t i = 0; i < output_cities.size(); ++i) {
                out << (componentList[output_cities[i]].id + 1) << (i + 1 < output_cities.size() ? " " : "");
            }
            out << "\n";
            // Write picked items (1-indexed)
            bool first_item = true;
            for (size_t i = 0; i < ultimate_packing.picked.size(); ++i) {
                if (ultimate_packing.picked[i]) {
                    if (!first_item) out << " ";
                    out << (items[i].id + 1);
                    first_item = false;
                }
            }
            out << "\n";
            out.close();
        } else {
            cerr << "Failed to open output file: " << p.outputfile << "\n";
        }
    }

    // Simple NN distance test
    if (componentList.size() > 0) {
        vector<bool> vis(componentList.size(), false);
        int curr = 0;
        vis[curr] = true;
        long double nn_dist = 0;
        for (size_t i = 1; i < componentList.size(); ++i) {
            int best_nxt = -1;
            long double best_d = 1e18;
            for (size_t j = 0; j < componentList.size(); ++j) {
                if (!vis[j]) {
                    long double d = connections.distance(componentList[curr], componentList[j]);
                    if (d < best_d) { best_d = d; best_nxt = j; }
                }
            }
            nn_dist += best_d;
            vis[best_nxt] = true;
            curr = best_nxt;
        }
        nn_dist += connections.distance(componentList[curr], componentList[0]);
        cout << "NN Distance: " << nn_dist << '\n';
    }

    cout << "Runtime: " << elapsed_time() << " s\n";

    if (originalCerrBuffer != nullptr) {
        cerr.rdbuf(originalCerrBuffer);
    }

    return 0;
}