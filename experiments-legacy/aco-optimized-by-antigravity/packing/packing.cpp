#include <bits/stdc++.h>
using namespace std;

// reference to https://cs.adelaide.edu.au/~optlog/research/ttp/2015gecco-ttp.pdf
// (m^2), mlogm 

struct Packing {
    vector<bool> picked;  
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
vector<bool> is_elite_city;
vector<int> elite_items; // Sorted list of best items (highest profit/weight)

double nu;

long double packing_distance(int i, int j) {
    return ceil(sqrt((nodes[i].x - nodes[j].x) * (nodes[i].x - nodes[j].x) +
            (nodes[i].y - nodes[j].y) * (nodes[i].y - nodes[j].y)));
}

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
    
    // --- Elite Cities Profiling ---
    is_elite_city.assign(nCities, false);
    vector<pair<double, int>> city_scores;
    for (int i = 0; i < nCities; ++i) {
        double best_ratio = 0;
        for (int item_idx : itemsAtCity[i]) {
            double ratio = (double)items[item_idx].profit / items[item_idx].weight;
            if (ratio > best_ratio) best_ratio = ratio;
        }
        city_scores.push_back({best_ratio, i});
    }
    sort(city_scores.rbegin(), city_scores.rend()); // descending
    int elite_city_count = max(1, (int)(nCities * 0.10));
    for (int i = 0; i < elite_city_count; ++i) {
        is_elite_city[city_scores[i].second] = true;
    }
    
    // --- Elite Items Profiling ---
    vector<pair<double, int>> item_scores;
    for (int i = 0; i < nItems; ++i) {
        item_scores.push_back({(double)items[i].profit / items[i].weight, i});
    }
    sort(item_scores.rbegin(), item_scores.rend());
    int elite_item_count = max(1, (int)(nItems * 0.20));
    for (int i = 0; i < elite_item_count; ++i) {
        elite_items.push_back(item_scores[i].second);
    }
    
    log_debug("packing: initialized city map, item buckets, and elite profiles");
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
        dist += packing_distance(tour[i], tour[i + 1]);
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

    // node -> distance to end
    vector<double> cityDistance(nCities, 0.0);

    for (int pos = 0; pos < (int)tour.size(); pos++)
        cityDistance[tour[pos]] = distToEnd[pos];

    score.resize(nItems);

    for (int item = 0; item < nItems; item++)
    {
        int node = cityIdToNodeIdx[items[item].city];

        double p = (double)items[item].profit;
        double w = (double)items[item].weight;
        double d = cityDistance[node];

        // avoid division by zero
        d = max(d, 1e-12);

        score[item] =
            pow(p, theta) /
            (
                pow(w, delta) *
                pow(d, gamma)
            );
    }
}

// EPSILON matching benchmark ACO++
static const double THOP_EPSILON = 0.00000000000000000000000000000001;

// Utilities =========================================================
// Compute time for the tour given current packing, matching benchmark logic:
// - no speed clamping (speed = maxSpeed - v*weight, allow going below minSpeed)
// - only visits cities with items picked (skips empties)
// - uses EPSILON tolerance for maxTime check
double computeFitness(const vector<int>& tour, Packing& packing){
    long weight = 0;
    long profit = 0;
    double time = 0.0;
    
    // Build weight accumulated per city
    vector<long> weight_at_city(nCities, 0);
    vector<long> profit_at_city(nCities, 0);
    for (int i = 0; i < nItems; ++i) {
        if (packing.picked[i]) {
            weight_at_city[items[i].city] += items[i].weight;
            profit_at_city[items[i].city] += items[i].profit;
            profit += items[i].profit;
            weight += items[i].weight;
        }
    }
    
    if (weight > capacity) return -INF;
    
    // Traverse tour benchmark-style: only visit cities with weight
    int prev_city = tour.front();
    long running_weight = 0;
    for (size_t i = 1; i < tour.size(); ++i) {
        int curr_city = tour[i];
        if (weight_at_city[curr_city] == 0 && curr_city != tour.back()) continue;
        double speed = maxSpeed - nu * running_weight;
        time += packing_distance(prev_city, curr_city) / speed;
        if (time - THOP_EPSILON > maxTime) return -INF;
        running_weight += weight_at_city[curr_city];
        prev_city = curr_city;
    }
    
    packing.totalWeight = weight;
    packing.totalProfit = profit;
    return profit;
}

Packing pack(const vector<int>& tour){
    Packing best;
    best.picked.assign(nItems, false);
    double bestProfit = -INF;

    // Map items to tour positions and precompute cumulative distances (double to avoid overflow)
    vector<int> item_to_pos(nItems, -1);
    vector<double> dist_acc(nCities, 0.0); // dist_acc[city] = cumulative dist from start to city
    double total_dist = 0.0;
    for (size_t i = 0; i + 1 < tour.size(); ++i) {
        dist_acc[tour[i]] = total_dist;
        total_dist += packing_distance(tour[i], tour[i+1]);
    }
    double total_tour_dist = total_dist;
    
    for (int i = 0; i < (int)tour.size(); ++i) {
        for (int item : itemsAtCity[tour[i]]) {
            item_to_pos[item] = i;
        }
    }

    // Phase 1: Random ptries restarts matching benchmark greedy (with per-item time check)
    for (int _try = 0; _try < params.ptries; ++_try) {
        // Random params (a,b,c), normalized
        double par_a = (double)rand() / RAND_MAX;
        double par_b = (double)rand() / RAND_MAX;
        double par_c = (double)rand() / RAND_MAX;
        double par_sum = par_a + par_b + par_c;
        if (par_sum < 1e-12) par_sum = 1.0;
        par_a /= par_sum; par_b /= par_sum; par_c /= par_sum;
        
        // Score items: -profit^a / (weight^b * dist_to_end^c)  (ascending = best first)
        vector<pair<double, int>> scored;
        scored.reserve(nItems);
        for (int i = 0; i < nItems; ++i) {
            if (item_to_pos[i] == -1) continue;
            double dist_to_end = total_tour_dist - dist_acc[items[i].city];
            if (dist_to_end < 1.0) dist_to_end = 1.0;
            double score = -1.0 * pow((double)items[i].profit, par_a)
                           / (pow((double)items[i].weight, par_b) * pow(dist_to_end, par_c));
            scored.push_back({score, i});
        }
        sort(scored.begin(), scored.end()); // ascending = most attractive first
        
        // Greedy: try each item, benchmark-style per-item full time feasibility check
        vector<long long> weight_at_city(nCities, 0);
        long long total_weight = 0, total_profit = 0;
        Packing cur;
        cur.picked.assign(nItems, false);
        
        for (int k = 0; k < (int)scored.size(); ++k) {
            int j = scored[k].second;
            if (total_weight + items[j].weight > capacity) continue;
            
            // Tentatively add item
            weight_at_city[items[j].city] += items[j].weight;
            
            // Per-item full time feasibility check (benchmark style)
            double _total_time = 0.0;
            long long _total_weight = 0;
            int prev_city = tour.front();
            bool violate = false;
            for (size_t i = 1; i < tour.size(); ++i) {
                int curr_city = tour[i];
                if (weight_at_city[curr_city] == 0 && curr_city != tour.back()) continue;
                double speed = maxSpeed - nu * (double)_total_weight;
                _total_time += packing_distance(prev_city, curr_city) / speed;
                if (_total_time - THOP_EPSILON > maxTime) { violate = true; break; }
                _total_weight += weight_at_city[curr_city];
                prev_city = curr_city;
            }
            
            if (!violate) {
                cur.picked[j] = true;
                total_profit += items[j].profit;
                total_weight += items[j].weight;
            } else {
                weight_at_city[items[j].city] -= items[j].weight;
            }
        }
        
        if ((double)total_profit > bestProfit) {
            bestProfit = (double)total_profit;
            best = cur;
        }
    }

    // Phase 2: 5 fixed param-set refinements
    vector<tuple<double, double, double>> fixed_params = {
        {1.0, 1.0, 1.0}, // standard
        {1.0, 2.0, 1.0}, // weight-averse
        {1.0, 1.0, 2.0}, // distance-averse
        {2.0, 1.0, 1.0}, // profit-focused
        {1.0, 3.0, 2.0}  // conservative
    };
    
    for (auto& p : fixed_params) {
        double theta = get<0>(p), delta = get<1>(p), gamma = get<2>(p);
        vector<double> score(nItems);
        computeScore(const_cast<vector<int>&>(tour), theta, delta, gamma, score);

        vector<int> order;
        for (int i = 0; i < nItems; ++i) if (item_to_pos[i] != -1) order.push_back(i);
        sort(order.begin(), order.end(), [&](int a, int b) { return score[a] > score[b]; });

        Packing cur;
        cur.picked.assign(nItems, false);
        for (int item : order) {
            cur.picked[item] = true;
            if (computeFitness(tour, cur) == -INF) {
                cur.picked[item] = false;
            }
        }
        
        double fp = computeFitness(tour, cur);
        if (fp > bestProfit) {
            bestProfit = fp;
            best = cur;
        }
    }

    // Fast Bit-Flip EA + Swap EA directly using computeFitness
    for (int iter = 0; iter < 5000; ++iter) {
        if (rand() % 5 != 0) { // Bit-Flip
            int r;
            if (!elite_items.empty() && rand() % 10 < 8) {
                r = elite_items[rand() % elite_items.size()];
            } else {
                r = rand() % nItems;
            }
            
            if (item_to_pos[r] == -1) continue;
            best.picked[r] = !best.picked[r];
            double p = computeFitness(tour, best);
            if (p != -INF && p >= bestProfit) {
                bestProfit = p;
            } else {
                best.picked[r] = !best.picked[r]; // revert
            }
        } else { // Swap
            int drop_item = rand() % nItems;
            if (!best.picked[drop_item] || item_to_pos[drop_item] == -1) continue;
            
            int add_item;
            if (!elite_items.empty() && rand() % 10 < 8) {
                add_item = elite_items[rand() % elite_items.size()];
            } else {
                add_item = rand() % nItems;
            }
            
            if (best.picked[add_item] || item_to_pos[add_item] == -1) continue;
            
            if (items[add_item].profit <= items[drop_item].profit) continue;
            
            best.picked[drop_item] = false;
            best.picked[add_item] = true;
            double p = computeFitness(tour, best);
            if (p != -INF && p > bestProfit) {
                bestProfit = p;
            } else {
                best.picked[drop_item] = true;
                best.picked[add_item] = false;
            }
        }
    }
    
    computeFitness(tour, best); // final sync
    return best;
}