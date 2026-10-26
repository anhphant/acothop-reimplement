#include <bits/stdc++.h>
using namespace std;

class Parameter {
public:
    double alpha1 = 1.0;
    double alpha2 = 1.0;
    double alpha3 = 1.0;
    double alpha4 = 1.0;
    double eps1 = 0.1;
    double eps2 = 0.1;
    double rho = 0.1;
    double delta = 1.0;
    bool flag_online_step_by_step_pheromone_update = false;
    bool flag_online_delayed_pheromone_update = false;
    string inputfile = "";
    string outputfile = "";
    int number_of_ants = 10;
    int number_of_packing = 0;
    int ptries = 1;
    int local_search_flag = 0; // 0: no local search   1: 2-opt   2: 2.5-opt   3: 3-opt
    double time_limit = 100.0;
    int seed = 1910;
    int nn_ants = 20;
    int nn_ls = 20;
    int esaco = 0;   // ESACO S1: pheromone-aware LS candidate injection (0=off, 1=on)
    int esaco2 = 0;  // ESACO S2: sparse pheromone store — evicted arc resets to tau_0
    int esaco3 = 0;  // ESACO S3: 3-tier transition rule (0=off, 1=on)
    string logfile = "";
} params;

struct Point { int id; double x, y; };

struct Item {
    long int id, profit, weight, city;
};

string name, knapsackType, edgeWeightType;
long nCities = 0, nItems = 0, capacity = 0;
double maxTime = 0, minSpeed = 0, maxSpeed = 0;

Point nodes[MAX_CITIES];
Item items[MAX_ITEMS];
double city_profit[MAX_CITIES];
double city_weight[MAX_CITIES];
// Top-K aggregation: sum profit/weight of top-K items by p/w ratio at each city.
// Used by compute_heuristic() instead of raw totals.
double city_profit_topk[MAX_CITIES];
double city_weight_topk[MAX_CITIES];

// Populate city_profit_topk / city_weight_topk.
// Call once after items[] are loaded and before compute_heuristic().
void compute_city_topk(int K = 8) {
    for (int i = 0; i < nCities; ++i) {
        city_profit_topk[i] = 0.0;
        city_weight_topk[i] = 0.0;
    }
    // Collect item indices per city, sort by p/w ratio descending, sum top K.
    static vector<int> buf[MAX_CITIES];
    for (int i = 0; i < nCities; ++i) buf[i].clear();
    for (int k = 0; k < nItems; ++k)
        buf[items[k].city].push_back(k);

    for (int i = 0; i < nCities; ++i) {
        auto& v = buf[i];
        sort(v.begin(), v.end(), [](int a, int b) {
            // sort descending by profit/weight (avoid division: cross-multiply)
            return (double)items[a].profit * items[b].weight
                 > (double)items[b].profit * items[a].weight;
        });
        int take = min((int)v.size(), K);
        for (int t = 0; t < take; ++t) {
            city_profit_topk[i] += items[v[t]].profit;
            city_weight_topk[i] += items[v[t]].weight;
        }
    }
}

// ========================================================

double distance(Point i, Point j) {
    double xd = i.x - j.x;
    double yd = i.y - j.y;
    double r = sqrt(xd * xd + yd * yd);

    if (edgeWeightType == "CEIL_2D" || edgeWeightType.find("CEIL_2D") != string::npos) {
        return ceil(r);
    }
    // Default to EUC_2D
    return r;
}

// On-the-fly CEIL_2D distance (no O(n^2) memo): recompute from coordinates.
// Dummy node (id == nCities) has no coordinates and is "far" from every city
// except start (0) and destination (nCities-1), matching the old memo semantics.
inline int distance(int i, int j) {
    if (i == j) return 0;
    const int dummy = (int)nCities;
    if (i == dummy || j == dummy) {
        int other = (i == dummy) ? j : i;
        if (other == 0 || other == (int)nCities - 1) return 0;
        return 1000000000;   // far: LS never pulls the dummy into the tour
    }
    return (int)distance(nodes[i], nodes[j]);
}

// Fractional knapsack upper bound, matching realbench read_thop_instance (inout.c:331-353).
static void ub_swap2(double *v, int *v2, int i, int j) {
    double tmp = v[i]; v[i] = v[j]; v[j] = tmp;
    int tmp2 = v2[i]; v2[i] = v2[j]; v2[j] = tmp2;
}
static void ub_sort2(double *v, int *v2, int left, int right) {
    int k, last;
    if (left >= right) return;
    ub_swap2(v, v2, left, (left + right) / 2);
    last = left;
    for (k = left + 1; k <= right; k++)
        if (v[k] < v[left]) ub_swap2(v, v2, ++last, k);
    ub_swap2(v, v2, left, last);
    ub_sort2(v, v2, left, last);
    ub_sort2(v, v2, last + 1, right);
}
double compute_upper_bound() {
    static double item_vector[MAX_ITEMS];
    static int help_vector[MAX_ITEMS];
    for (int j = 0; j < nItems; ++j) {
        item_vector[j] = (-1.0 * items[j].profit) / items[j].weight;
        help_vector[j] = j;
    }
    ub_sort2(item_vector, help_vector, 0, nItems - 1);
    double UB = 0;
    long int _w = 0;
    for (int k = 0; k < nItems; ++k) {
        int j = help_vector[k];
        if (_w + items[j].weight <= capacity) {
            _w += items[j].weight;
            UB += items[j].profit;
        } else {
            UB += ceil((capacity - _w) / (double)items[j].weight * items[j].profit);
            break;
        }
    }
    return UB;
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

    for (long i = 0; i < nCities; ++i) {
        fin >> nodes[i].id >> nodes[i].x >> nodes[i].y;
        nodes[i].id--;
    }

    fin.ignore(256, '\n');
    skip();                      // ITEMS SECTION

    for (long k = 0; k < nItems; ++k) {
        fin >> items[k].id >> items[k].profit >> items[k].weight >> items[k].city;
        items[k].id--;
        items[k].city--;
    }

    for (int i = 0; i <= nCities; ++i) {
        city_profit[i] = 0.0;
        city_weight[i] = 0.0;
    }
    for (long k = 0; k < nItems; ++k) {
        city_profit[items[k].city] += items[k].profit;
        city_weight[items[k].city] += items[k].weight;
    }

    log_debug("thopproblem: read header -> cities=", nCities,
         ", items=", nItems, ", capacity=", capacity,
         ", maxTime=", maxTime);
    log_debug("thopproblem: loaded ", nCities, " nodes and ",
         nItems, " items");
    return true;
}
