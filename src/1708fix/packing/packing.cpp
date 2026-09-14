#include <bits/stdc++.h>
using namespace std;

// reference to https://cs.adelaide.edu.au/~optlog/research/ttp/2015gecco-ttp.pdf
// (m^2), mlogm 

struct Packing {
    bitset<MAX_ITEMS> picked;  
    long totalWeight = 0, totalProfit = 0;
};

struct Solution {
    int tour[MAX_CITIES];    
    int tour_len;
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


vector<int> cityIdToNodeIdx; // Map from 1-based city ID to 0-based node index

int itemsAtCityArr[MAX_CITIES][100];
int numItemsAtCity[MAX_CITIES];

double nu;

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
    for (int i=0; i<MAX_CITIES; i++) numItemsAtCity[i] = 0;
    
    for (long k = 0; k < nItems; ++k) {
        int nodeIdx = cityIdToNodeIdx[items[k].city];
        if (nodeIdx != -1) {
            if (numItemsAtCity[nodeIdx] < 100) {
                itemsAtCityArr[nodeIdx][numItemsAtCity[nodeIdx]++] = k;
            }
        }
    }

    // --- speed / capacity constants for evaluate() ---
    nu = (maxSpeed - minSpeed) / capacity;   // ν = speed decay per unit weight
    log_debug("packing: initialized city map and item buckets");
}

void computeDistanceToEnd(const int* tour, int tour_size,
                          vector<double>& distanceToEnd)
{
    distanceToEnd.assign(tour_size, 0.0);
    double dist = 0.0;
    distanceToEnd[tour_size - 1] = 0.0;   // end city

    for (int i = tour_size - 2; i >= 0; i--)
    {
        dist += distance(tour[i], tour[i + 1]);
        distanceToEnd[i] = dist;
    }
}

// ========================================================================

void computeScore(const int* tour, int tour_size,
                  double theta,
                  double delta,
                  double gamma,
                  double* score)
{
    // distance from each position in tour to end city
    vector<double> distToEnd;
    computeDistanceToEnd(tour, tour_size, distToEnd);

    double totalTourDist = distToEnd.empty() ? 1.0 : distToEnd.front();
    if (totalTourDist <= 0.0) totalTourDist = 1.0;

    // node -> distance to end
    vector<double> cityDistance(nCities, 0.0);

    for (int pos = 0; pos < tour_size; pos++)
        cityDistance[tour[pos]] = distToEnd[pos];

    // score is static array of size MAX_ITEMS, so no need to resize

    for (int item = 0; item < nItems; item++)
    {
        int node = cityIdToNodeIdx[items[item].city];

        double p = (double)items[item].profit;
        double w = (double)items[item].weight;

        // Normalize distance ratio: in (0, 1]
        double d_ratio = cityDistance[node] / totalTourDist;

        score[item] = pow(p, theta) / (pow(w, delta) * pow(d_ratio + 0.01, gamma));
    }
}

// Utilities =========================================================
double computeTime(const int* tour, int tour_size, const Packing& packing) {
    double time = 0.0;
    long currentWeight = 0;

    int currentCity = tour[0];
    
    for (int i = 0; i < numItemsAtCity[currentCity]; i++) {
        if (packing.picked.test(itemsAtCityArr[currentCity][i])) {
            currentWeight += items[itemsAtCityArr[currentCity][i]].weight;
        }
    }

    for (int i = 1; i < tour_size; i++) {
        int nextCity = tour[i];
        
        double v = maxSpeed - currentWeight * nu;
        
        time += distance(currentCity, nextCity) / v;

        for (int j = 0; j < numItemsAtCity[nextCity]; j++) {
            if (packing.picked.test(itemsAtCityArr[nextCity][j])) {
                currentWeight += items[itemsAtCityArr[nextCity][j]].weight;
            }
        }

        currentCity = nextCity;
    }

    return time;
}

double computeFitness(const int* tour, int tour_size, Packing& packing) {
    double time = computeTime(tour, tour_size, packing);
    if (time > maxTime) return -INF;
    return (double)packing.totalProfit;
}

void polishPacking(const int* tour, int tour_size, Packing& packing, double& bestProfit) {
    static int visitedCities[MAX_CITIES];
    int visited_count = 0;
    for (int i = 0; i < tour_size; i++) {
        int city = tour[i];
        for (int k = 0; k < numItemsAtCity[city]; k++) {
            int item = itemsAtCityArr[city][k];
            if (packing.picked.test(item)) {
                visitedCities[visited_count++] = city;
                break;
            }
        }
    }
    
    for (int i = 0; i < visited_count / 2; i++) {
        swap(visitedCities[i], visitedCities[visited_count - 1 - i]);
    }
    
    for (int i = 0; i < visited_count; i++) {
        int city = visitedCities[i];
        static int unpicked[MAX_ITEMS];
        int unpicked_len = 0;
        for (int k = 0; k < numItemsAtCity[city]; k++) {
            int item = itemsAtCityArr[city][k];
            if (!packing.picked.test(item)) {
                unpicked[unpicked_len++] = item;
            }
        }
        sort(unpicked, unpicked + unpicked_len, [](int a, int b) {
            return (double)items[a].profit / items[a].weight > (double)items[b].profit / items[b].weight;
        });
        
        for (int k = 0; k < unpicked_len; k++) {
            int item = unpicked[k];
            if (packing.totalWeight + items[item].weight <= capacity) {
                packing.picked.set(item);
                packing.totalWeight += items[item].weight;
                packing.totalProfit += items[item].profit;
                
                double f = computeFitness(tour, tour_size, packing);
                if (f > bestProfit) {
                    bestProfit = f;
                } else {
                    packing.picked.reset(item);
                    packing.totalWeight -= items[item].weight;
                    packing.totalProfit -= items[item].profit;
                }
            }
        }
    }
}

void lateHarvestOptimization(const int* tour, int tour_size, Packing& packing, double& bestProfit) {
    bool improved = true;
    int max_rounds = 2;
    int round = 0;
    while (improved && ++round <= max_rounds) {
        improved = false;
        int cutoff = tour_size * 6 / 10;
        static int early_picked[MAX_ITEMS];
        int early_picked_len = 0;
        for (int i = 0; i < cutoff; ++i) {
            int city = tour[i];
            for (int k = 0; k < numItemsAtCity[city]; k++) {
                int item = itemsAtCityArr[city][k];
                if (packing.picked.test(item)) early_picked[early_picked_len++] = item;
            }
        }
        sort(early_picked, early_picked + early_picked_len, [](int a, int b) {
            return (double)items[a].profit / items[a].weight < (double)items[b].profit / items[b].weight;
        });

        if (early_picked_len > 15) early_picked_len = 15;

        for (int i = 0; i < early_picked_len; i++) {
            int drop_item = early_picked[i];
            Packing test_packing = packing;
            test_packing.picked.reset(drop_item);
            test_packing.totalWeight -= items[drop_item].weight;
            test_packing.totalProfit -= items[drop_item].profit;
            
            double cur_p = computeFitness(tour, tour_size, test_packing);
            if (cur_p == -INF) {
                continue;
            }

            polishPacking(tour, tour_size, test_packing, cur_p);

            if (cur_p > bestProfit) {
                bestProfit = cur_p;
                packing = test_packing;
                improved = true;
                break;
            }
        }
    }
}

Packing pack(const int* tour, int tour_size, int override_ptries = -1){
    Packing best;
    best.picked.reset();
    best.totalWeight = 0;
    best.totalProfit = 0;
    double bestProfit = -INF;
    mt19937 rng(rand());
    uniform_real_distribution<double> dist(0.0, 1.0);
    static double score[MAX_ITEMS];

    int actual_ptries = (override_ptries == -1) ? packParams.ptries : override_ptries;
    for (int attempt = 0; attempt < actual_ptries; attempt++)
    {
        double theta = dist(rng) * 2.5 + 0.5;
        double delta = dist(rng) * 2.5 + 0.5;
        double gamma = dist(rng) * 3.5 + 0.5;

        computeScore(tour, tour_size, theta, delta, gamma, score);

        static int order[MAX_ITEMS];
        iota(order, order + nItems, 0);
        sort(order, order + nItems, [&](int a, int b) { return score[a] > score[b]; });

        int low = 0, high = nItems, best_k = 0;
        while (low <= high) {
            int mid = low + (high - low) / 2;
            Packing test_cur;
            test_cur.picked.reset();
            long test_weight = 0;
            for (int i = 0; i < mid; ++i) {
                if (test_weight + items[order[i]].weight <= capacity) {
                    test_cur.picked.set(order[i]);
                    test_weight += items[order[i]].weight;
                }
            }
            if (computeTime(tour, tour_size, test_cur) <= maxTime) {
                best_k = mid;
                low = mid + 1;
            } else {
                high = mid - 1;
            }
        }
        
        Packing cur;
        cur.picked.reset();
        cur.totalWeight = 0;
        cur.totalProfit = 0;
        for (int i = 0; i < best_k; ++i) {
            int item = order[i];
            if (cur.totalWeight + items[item].weight <= capacity) {
                cur.picked.set(item);
                cur.totalWeight += items[item].weight;
                cur.totalProfit += items[item].profit;
            }
        }

        double profit = computeFitness(tour, tour_size, cur);
        if (profit > bestProfit) {
            bestProfit = profit;
            best = cur;
        }
    }

    polishPacking(tour, tour_size, best, bestProfit);

    // Late harvest optimization
    if (packParams.ptries > 5) {
        lateHarvestOptimization(tour, tour_size, best, bestProfit);
    }
    
    return best;
}