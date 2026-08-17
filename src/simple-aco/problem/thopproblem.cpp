#include <bits/stdc++.h>
using namespace std;

class Parameter {
public:
    long double alpha = 1.0L;
    long double beta = 1.0L;
    long double rho = 0.1L;
    long double delta = 1.0L;
    bool flag_online_step_by_step_pheromone_update = false;
    bool flag_online_delayed_pheromone_update = false;
    string inputfile = "";
    string outputfile = "";
    int number_of_ants = 10;
    int number_of_packing = 0;
    int ptries = 1;
    int local_search_flag = 0; // 0: no local search   1: 2-opt   2: 2.5-opt   3: 3-opt
    long double time_limit = 100.0L;
    int seed = 1910;
    string logfile = "";
} params;

struct Point { int id; double x, y; };

struct Item {
    long int id, profit, weight, city;
};

string name, knapsackType, edgeWeightType;
long nCities = 0, nItems = 0, capacity = 0;
double maxTime = 0, minSpeed = 0, maxSpeed = 0;

vector<Point> nodes;
vector<Item> items;

// ========================================================

long double distance(Point i, Point j) {
    double xd = i.x - j.x;
    double yd = i.y - j.y;
    double r = sqrt(xd * xd + yd * yd);
    
    if (edgeWeightType == "CEIL_2D" || edgeWeightType.find("CEIL_2D") != string::npos) {
        return ceil(r);
    }
    // Default to EUC_2D
    return r;
}

vector<vector<long double>> dist_memo;

long double distance(int i, int j) {
    if (dist_memo.empty()) {
        dist_memo.assign(nodes.size(), vector<long double>(nodes.size(), -1.0));
    }
    if (dist_memo[i][j] < 0) {
        long double d = distance(nodes[i], nodes[j]);
        dist_memo[i][j] = d;
        dist_memo[j][i] = d;
        return d;
    }
    return dist_memo[i][j];
}

bool readData(const string& path) {
    log_debug("thopproblem: loading data from ", path);
    ifstream fin(path);
    if (!fin) {
        log_debug("thopproblem: failed to open input file ", path);
        return false;
    }

    string line;

    auto skip = [&] { getline(fin, line); };
    auto take_after_prefix = [&](size_t prefix) {
        return line.size() > prefix ? line.substr(prefix) : string{};
    };

    skip(); name = take_after_prefix(14);
    skip(); knapsackType = take_after_prefix(19);

    fin.ignore(256, ':'); fin >> nCities;
    fin.ignore(256, ':'); fin >> nItems;
    fin.ignore(256, ':'); fin >> capacity;
    fin.ignore(256, ':'); fin >> maxTime;
    fin.ignore(256, ':'); fin >> minSpeed;
    fin.ignore(256, ':'); fin >> maxSpeed;

    fin.ignore(256, '\n');
    skip(); edgeWeightType = line.substr(17);

    skip();                      // NODE_COORD_SECTION

    nodes.resize(nCities);
    for (auto& p : nodes) {
        fin >> p.id >> p.x >> p.y;
        p.id--;
    }

    fin.ignore(256, '\n');
    skip();                      // ITEMS SECTION

    items.resize(nItems);
    for (auto& item : items) {
        fin >> item.id >> item.profit >> item.weight >> item.city;
        item.id--;
        item.city--;
    }

    log_debug("thopproblem: read header -> cities=", nCities,
         ", items=", nItems, ", capacity=", capacity,
         ", maxTime=", maxTime);
    log_debug("thopproblem: loaded ", nodes.size(), " nodes and ",
         items.size(), " items");
    return true;
}
