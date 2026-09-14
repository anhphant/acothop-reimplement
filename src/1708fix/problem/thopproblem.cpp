#include <bits/stdc++.h>
using namespace std;

const int MAX_CITIES = 10005;
const int MAX_ITEMS = 100005;
class Parameter {
public:
    double alpha1 = 1.0L;
    double alpha2 = 1.0L;
    double alpha3 = 1.0L;
    double alpha4 = 1.0L;
    double rho = 0.1L;
    double delta = 1.0L;
    bool flag_online_step_by_step_pheromone_update = false;
    bool flag_online_delayed_pheromone_update = false;
    string inputfile = "";
    string outputfile = "";
    int number_of_ants = 10;
    int number_of_packing = 0;
    int ptries = 1;
    int local_search_flag = 0; // 0: no local search   1: 2-opt   2: 2.5-opt   3: 3-opt
    double time_limit = 100.0L;
    int seed = 1910;
    string logfile = "";
    bool adaptive_heuristic = false;
} params;

struct Point { int id; double x, y; double totalWeight = 0.0; double totalProfit = 0.0; };

struct Item {
    long int id, profit, weight, city;
};

string name, knapsackType, edgeWeightType;
long nCities = 0, nItems = 0, capacity = 0;
double maxTime = 0, minSpeed = 0, maxSpeed = 0;

Point nodes[MAX_CITIES];
Item items[MAX_ITEMS];

double distance(Point i, Point j) {
    double r = ceil(sqrt(pow((double)i.x - j.x, 2) + pow((double)i.y - j.y, 2)));
    return r;
}

double dist_memo[MAX_CITIES][MAX_CITIES];

double distance(int i, int j) {
    if (dist_memo[i][j] < 0) {
        double d = distance(nodes[i], nodes[j]);
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

    for (int i = 0; i < nCities; ++i) {
        Point& p = nodes[i];
        fin >> p.id >> p.x >> p.y;
        p.id--;
    }

    fin.ignore(256, '\n');
    skip();                      // ITEMS SECTION

    for (int i = 0; i < nItems; ++i) {
        Item& item = items[i];
        fin >> item.id >> item.profit >> item.weight >> item.city;
        item.id--;
        item.city--;
        nodes[item.city].totalWeight += item.weight;
        nodes[item.city].totalProfit += item.profit;
    }

    fin.close();

    for (int i = 0; i < nCities; ++i) {
        for (int j = 0; j < nCities; ++j) {
            dist_memo[i][j] = -1.0;
        }
    }

    log_debug("thopproblem: read header -> cities=", nCities,
         ", items=", nItems, ", capacity=", capacity,
         ", maxTime=", maxTime);
    log_debug("thopproblem: loaded ", nCities, " nodes and ",
         nItems, " items");
    log_debug("thopproblem: Node 0 -> id: ", nodes[0].id, " x: ", nodes[0].x, " y: ", nodes[0].y);
    log_debug("thopproblem: Node 999 -> id: ", nodes[999].id, " x: ", nodes[999].x, " y: ", nodes[999].y);
    return true;
}
