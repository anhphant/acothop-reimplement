#include <bits/stdc++.h>
using namespace std;

// reference to https://cs.adelaide.edu.au/~optlog/research/ttp/2015gecco-ttp.pdf
// (m^2), mlogm 

struct Packing {
    vector<bool> picked;  
    vector<long> weight_at_city;
    long totalWeight = 0, totalProfit = 0;
};

struct Solution {
    vector<int> tour;    
    Packing packing;
    double objective = -INF;
};

// struct packParameter {
//     double alpha = 1.0;     // exponent for profit & weight in scoring (α = β per paper)
//     double c = 5.0;         // starting exponent for PackIterative ternary search
//     double delta = 2.5;     // initial half-width for ternary search interval
//     double epsilon = 0.1;   // early-stop threshold for PackIterative
//     int q = 20;             // max iterations for PackIterative
//     int tau = 100;          // φ: frequency divisor ε = m/φ in Algorithm 1
//     int eaIteration = 10000; // max iterations for (1+1)-EA
// } packParams;
struct PackParameter {
    int ptries = 20;
} packParams;


vector<vector<int>> itemsAtCity;   // itemsAtCity[nodeIdx] = list of item indices
vector<int> cityIdToNodeIdx; // Map from 1-based city ID to 0-based node index

double nu;

// Global vectors for computeFitness to avoid reallocations
vector<int> global_visitedCities;
vector<int> global_path;
vector<bool> global_alreadyPicked;

vector<double> log_p;
vector<double> log_w;

void packInit() {
    log_debug("packing: initializing packing structures");
    log_debug("packing: nCities=", nCities, ", nItems=", nItems,
         ", capacity=", capacity, ", minSpeed=", minSpeed,
         ", maxSpeed=", maxSpeed);

    // build city-ID 
    cityIdToNodeIdx.assign(nCities, -1);
    for (long i = 0; i < nCities; ++i) {
        cityIdToNodeIdx[nodes[i].id] = (int)i;
    }

    // build itemsAtCity 
    itemsAtCity.assign(nCities, vector<int>());
    for (long k = 0; k < nItems; ++k) {
        int nodeIdx = cityIdToNodeIdx[items[k].city];
        if (nodeIdx != -1) {
            itemsAtCity[nodeIdx].push_back((int)k);
        }
    }

    // --- speed / capacity constants for evaluate() ---
    nu = (maxSpeed - minSpeed) / capacity;   // ν = speed decay per unit weight
    
    // reserve memory for global vectors
    global_visitedCities.reserve(nCities);
    global_path.reserve(nCities + 2);
    global_alreadyPicked.assign(nItems, false);
    
    log_p.assign(nItems, 0.0);
    log_w.assign(nItems, 0.0);
    for (long k = 0; k < nItems; ++k) {
        log_p[k] = log((double)max(items[k].profit, 1L));
        log_w[k] = log((double)max(items[k].weight, 1L));
    }
    
    log_debug("packing: initialized city map and item buckets");
}

void computeDistanceToEnd(const vector<int>& tour,
                          vector<double>& distanceToEnd)
{
    int n = tour.size();

    distanceToEnd.assign(n, 0.0);

    double dist = 0.0;

    distanceToEnd[n - 1] = 0.0;   // end city

    for (int i = n - 2; i >= 0; i--)
    {
        dist += distance(tour[i], tour[i + 1]);
        distanceToEnd[i] = dist;
    }
}

// ========================================================================

void computeScore(const vector<int>& tour,
                  double theta,
                  double delta,
                  double gamma,
                  vector<double>& score)
{
    // distance from each position in tour to end city
    vector<double> distToEnd;
    computeDistanceToEnd(const_cast<vector<int>&>(tour), distToEnd);

    vector<double> cityDistance(nCities, -1.0);

    for (int pos = 0; pos < (int)tour.size(); pos++)
        cityDistance[tour[pos]] = distToEnd[pos];

    score.resize(nItems);

    for (int item = 0; item < nItems; item++)
    {
        int node = cityIdToNodeIdx[items[item].city];
        double d = cityDistance[node];
        
        if (d < 0.0) {
            score[item] = -INF;
            continue;
        }
        
        double log_d = log(max(d, 1e-12));

        // Use log-score for ranking instead of exp to save computation
        score[item] = theta * log_p[item] - delta * log_w[item] - gamma * log_d;
    }
}

// Utilities =========================================================
double computeFitness(const vector<int>& tour, const Packing& packing){
    if (packing.totalWeight > capacity) return -INF;

    long weight = 0;
    double time = 0.0;

    global_path.clear();
    global_path.push_back(tour.front());

    for (int city : tour)
    {
        if (packing.weight_at_city[city] > 0)
        {
            global_path.push_back(city);
        }
    }

    if (global_path.back() != tour.back())
        global_path.push_back(tour.back());

    // simulate travelling
    for (size_t i = 0; i + 1 < global_path.size(); i++)
    {
        int city = global_path[i];
        weight += packing.weight_at_city[city];

        double speed = maxSpeed - nu * weight;

        if (speed < minSpeed)
            speed = minSpeed;

        time += distance(city, global_path[i + 1]) / speed;

        if (time > maxTime)
            return -INF;
    }

    return (double)packing.totalProfit;
}

Packing pack(const vector<int>& tour){
    auto t_start = chrono::steady_clock::now();
    double ms_score = 0, ms_sort = 0, ms_greedy = 0, ms_fit = 0;
    
    Packing best;
    best.picked.assign(nItems, false);
    double bestProfit = -INF;
    vector<double> score(nItems);

    for (int attempt = 0; attempt < packParams.ptries; attempt++)
    {
        auto t0 = chrono::steady_clock::now();
        double theta = rand_prob(rng);
        double delta = rand_prob(rng);
        double gamma = rand_prob(rng);
        double sum = theta + delta + gamma;
        theta /= sum; delta /= sum; gamma /= sum;

        computeScore(
            const_cast<vector<int>&>(tour),
            theta, delta, gamma, score);

        auto t1 = chrono::steady_clock::now();
        vector<int> order(nItems);
        iota(order.begin(), order.end(), 0);
        sort(order.begin(), order.end(),
            [&](int a, int b) { return score[a] > score[b]; });

        auto t2 = chrono::steady_clock::now();
        Packing cur;
        cur.picked.assign(nItems, false);
        cur.weight_at_city.assign(nCities, 0);
        cur.totalWeight = 0;
        cur.totalProfit = 0;

        vector<int> kept_items;
        for (int item : order) {
            if (score[item] == -INF) continue;
            if (cur.totalWeight + items[item].weight > capacity) continue;
            kept_items.push_back(item);
            cur.totalWeight += items[item].weight; // Must update cur.totalWeight during greedy construction!
        }

        int low = 0;
        int high = (int)kept_items.size();
        int best_k = 0;

        while (low <= high) {
            int mid = low + (high - low) / 2;
            
            // Construct packing with first 'mid' items
            Packing test_cur;
            test_cur.picked.assign(nItems, false);
            test_cur.weight_at_city.assign(nCities, 0);
            test_cur.totalWeight = 0;
            test_cur.totalProfit = 0;

            for (int i = 0; i < mid; i++) {
                int item = kept_items[i];
                test_cur.picked[item] = true;
                test_cur.totalWeight += items[item].weight;
                test_cur.totalProfit += items[item].profit;
                test_cur.weight_at_city[items[item].city] += items[item].weight;
            }

            if (computeFitness(tour, test_cur) != -INF) {
                best_k = mid;
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }

        // Reconstruct best cur
        cur.picked.assign(nItems, false);
        cur.weight_at_city.assign(nCities, 0);
        cur.totalWeight = 0;
        cur.totalProfit = 0;
        for (int i = 0; i < best_k; i++) {
            int item = kept_items[i];
            cur.picked[item] = true;
            cur.totalWeight += items[item].weight;
            cur.totalProfit += items[item].profit;
            cur.weight_at_city[items[item].city] += items[item].weight;
        }

        double profit = (double)cur.totalProfit;
        if (profit > bestProfit) {
            bestProfit = profit;
            best = cur;
        }
    }

    double ms_total = chrono::duration<double, milli>(chrono::steady_clock::now() - t_start).count();
    if (ms_total > 5.0) {
        log_debug("pack timings: score=", ms_score, "ms, sort=", ms_sort, "ms, greedy=", ms_greedy, "ms, fit=", ms_fit, "ms, total=", ms_total, "ms");
    }
    return best;
}