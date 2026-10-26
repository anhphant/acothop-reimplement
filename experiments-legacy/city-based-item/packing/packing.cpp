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

// packing_distance removed

#include "pack_iterative.h"

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

    double totalTourDist = distToEnd.empty() ? 1.0 : distToEnd.front();
    if (totalTourDist <= 0.0) totalTourDist = 1.0;

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

        // Normalize distance ratio: in (0, 1]
        double d_ratio = d / totalTourDist;

        // Refined score formula: Profit ^ theta / (Weight ^ delta * (d_ratio + 0.01) ^ gamma)
        score[item] =
            pow(p, theta) /
            (
                pow(w, delta) *
                pow(d_ratio + 0.01, gamma)
            );
    }
}

// Utilities =========================================================
double computeFitness(const vector<int>& tour, Packing& packing){
    long weight = 0;
    long profit = 0;
    double time = 0.0;

    int prevCity = tour.front();

    // cities visited after removing empty cities
    vector<int> visitedCities;

    for (int city : tour)
    {
        bool hasPicked = false;

        for (int item : itemsAtCity[city])
        {
            if (packing.picked[item])
            {
                hasPicked = true;
                break;
            }
        }

        if (hasPicked)
            visitedCities.push_back(city);
    }

    // start -> picked cities -> end
    vector<int> path;
    path.push_back(tour.front());

    for (int c : visitedCities)
        path.push_back(c);

    if (path.back() != tour.back())
        path.push_back(tour.back());

    vector<bool> alreadyPicked(nItems, false);

    // simulate travelling
    for (size_t i = 0; i + 1 < path.size(); i++)
    {
        int city = path[i];

        // pick all items at current city
        for (int item : itemsAtCity[city])
        {
            if (!packing.picked[item]) continue;
            if (alreadyPicked[item]) continue;
            alreadyPicked[item] = true;

            weight += items[item].weight;

            if (weight > capacity)
                return -INF;

            profit += items[item].profit;
        }

        double speed = maxSpeed - nu * weight;

        if (speed < minSpeed)
            speed = minSpeed;

        time += distance(city, path[i + 1]) / speed;
    }

    if (time > maxTime)
        return -INF;

    packing.totalWeight = weight;
    packing.totalProfit = profit;

    // Uncomment to debug time discrepancy
    // cout << "C++ computeFitness time: " << fixed << time << " maxTime: " << maxTime << endl;

    return profit;
}

void polishPacking(const vector<int>& tour, Packing& packing, double& bestProfit) {
    vector<int> visitedCities;
    for (int city : tour) {
        for (int item : itemsAtCity[city]) {
            if (packing.picked[item]) {
                visitedCities.push_back(city);
                break;
            }
        }
    }
    reverse(visitedCities.begin(), visitedCities.end());
    for (int city : visitedCities) {
        vector<int> unpicked;
        for (int item : itemsAtCity[city]) {
            if (!packing.picked[item]) unpicked.push_back(item);
        }
        sort(unpicked.begin(), unpicked.end(), [](int a, int b) {
            return (double)items[a].profit / items[a].weight > (double)items[b].profit / items[b].weight;
        });
        for (int item : unpicked) {
            if (packing.totalWeight + items[item].weight <= capacity) {
                packing.picked[item] = true;
                double f = computeFitness(tour, packing);
                if (f > bestProfit) {
                    bestProfit = f;
                } else {
                    packing.picked[item] = false;
                }
            }
        }
    }
}

void lateHarvestOptimization(const vector<int>& tour, Packing& packing, double& bestProfit) {
    bool improved = true;
    int max_rounds = 2;
    int round = 0;
    while (improved && ++round <= max_rounds) {
        improved = false;
        size_t cutoff = tour.size() * 6 / 10;
        vector<int> early_picked;
        for (size_t i = 0; i < cutoff; ++i) {
            int city = tour[i];
            for (int item : itemsAtCity[city]) {
                if (packing.picked[item]) early_picked.push_back(item);
            }
        }
        sort(early_picked.begin(), early_picked.end(), [](int a, int b) {
            return (double)items[a].profit / items[a].weight < (double)items[b].profit / items[b].weight;
        });

        if (early_picked.size() > 15) early_picked.resize(15);

        for (int drop_item : early_picked) {
            packing.picked[drop_item] = false;
            double cur_p = computeFitness(tour, packing);
            if (cur_p == -INF) {
                packing.picked[drop_item] = true;
                continue;
            }

            polishPacking(tour, packing, cur_p);

            if (cur_p > bestProfit) {
                bestProfit = cur_p;
                improved = true;
                break;
            } else {
                packing.picked[drop_item] = true;
                polishPacking(tour, packing, bestProfit);
            }
        }
    }
}

Packing pack(const vector<int>& tour, int override_ptries = -1){
    Packing best;
    best.picked.assign(nItems, false);

    double bestProfit = -INF;

    mt19937 rng(rand());

    uniform_real_distribution<double> dist(0.0, 1.0);

    vector<double> score(nItems);

    int actual_ptries = (override_ptries == -1) ? packParams.ptries : override_ptries;
    for (int attempt = 0; attempt < actual_ptries; attempt++)
    {
        //---------------------------------------
        // dynamic θ δ γ emphasizing late harvest
        //---------------------------------------

        double theta = dist(rng) * 2.5 + 0.5; // [0.5, 3.0]
        double delta = dist(rng) * 2.5 + 0.5; // [0.5, 3.0]
        double gamma = dist(rng) * 3.5 + 0.5; // [0.5, 4.0]

        //---------------------------------------
        // compute scores
        //---------------------------------------

        computeScore(
            const_cast<vector<int>&>(tour),
            theta,
            delta,
            gamma,
            score);

        vector<int> order(nItems);

        iota(order.begin(), order.end(), 0);

        sort(order.begin(), order.end(),
            [&](int a, int b)
            {
                return score[a] > score[b];
            });

        //---------------------------------------
        // greedy
        //---------------------------------------

        int low = 0, high = order.size(), best_k = 0;
        while (low <= high) {
            int mid = low + (high - low) / 2;
            
            Packing test_cur;
            test_cur.picked.assign(nItems, false);
            long test_weight = 0;
            
            for (int i = 0; i < mid; ++i) {
                int item = order[i];
                if (test_weight + items[item].weight <= capacity) {
                    test_cur.picked[item] = true;
                    test_weight += items[item].weight;
                }
            }
            
            if (computeFitness(tour, test_cur) != -INF) {
                best_k = mid;
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }
        
        Packing cur;
        cur.picked.assign(nItems, false);
        long currentWeight = 0;
        for (int i = 0; i < best_k; ++i) {
            int item = order[i];
            if (currentWeight + items[item].weight <= capacity) {
                cur.picked[item] = true;
                currentWeight += items[item].weight;
            }
        }

        double profit = computeFitness(tour, cur);

        if (profit > bestProfit)
        {
            bestProfit = profit;
            best = cur;
        }
    }

    // Polish best packing with greedy fill on visited cities
    polishPacking(tour, best, bestProfit);

    // Late harvest optimization
    if (packParams.ptries > 5) {
        lateHarvestOptimization(tour, best, bestProfit);
    }
    
    return best;
}