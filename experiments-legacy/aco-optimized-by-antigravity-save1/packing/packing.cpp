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

double nu;

long double packing_distance(int i, int j) {
    return sqrt((nodes[i].x - nodes[j].x) * (nodes[i].x - nodes[j].x) +
            (nodes[i].y - nodes[j].y) * (nodes[i].y - nodes[j].y));
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

// Utilities =========================================================
double computeFitness(const vector<int>& tour, Packing& packing){
    long weight = 0;
    long profit = 0;
    double time = 0.0;
    
    int last_city = tour.front();
    vector<bool> alreadyPicked(nItems, false);
    
    for (int city : tour) {
        long city_weight = 0;
        long city_profit = 0;
        bool has_picked = false;
        
        for (int item : itemsAtCity[city]) {
            if (packing.picked[item] && !alreadyPicked[item]) {
                alreadyPicked[item] = true;
                city_weight += items[item].weight;
                city_profit += items[item].profit;
                has_picked = true;
            }
        }
        
        if (has_picked) {
            if (last_city != city) {
                double speed = maxSpeed - nu * weight;
                if (speed < minSpeed) speed = minSpeed;
                time += packing_distance(last_city, city) / speed;
                last_city = city;
            }
            weight += city_weight;
            if (weight > capacity) return -INF;
            profit += city_profit;
        }
    }
    
    if (last_city != tour.back()) {
        double speed = maxSpeed - nu * weight;
        if (speed < minSpeed) speed = minSpeed;
        time += packing_distance(last_city, tour.back()) / speed;
    }
    
    if (time > maxTime) return -INF;
    
    packing.totalWeight = weight;
    packing.totalProfit = profit;
    return profit;
}

Packing pack(const vector<int>& tour){
    Packing best;
    best.picked.assign(nItems, false);
    double bestProfit = -INF;

    // Map items to tour positions
    vector<int> item_to_pos(nItems, -1);
    for (int i = 0; i < (int)tour.size(); ++i) {
        for (int item : itemsAtCity[tour[i]]) {
            item_to_pos[item] = i;
        }
    }

    vector<tuple<double, double, double>> params_to_try = {
        {1.0, 1.0, 1.0}, // standard
        {1.0, 2.0, 1.0}, // weight-averse
        {1.0, 1.0, 2.0}, // distance-averse
        {2.0, 1.0, 1.0}, // profit-focused
        {1.0, 3.0, 2.0}  // conservative
    };
    
    for (auto& params : params_to_try) {
        double theta = get<0>(params), delta = get<1>(params), gamma = get<2>(params);
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
        
        double p = computeFitness(tour, cur);
        if (p > bestProfit) {
            bestProfit = p;
            best = cur;
        }
    }

    // Fast Bit-Flip EA + Swap EA directly using computeFitness
    for (int iter = 0; iter < 5000; ++iter) {
        if (rand() % 5 != 0) { // Bit-Flip
            int r = rand() % nItems;
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
            int add_item = rand() % nItems;
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