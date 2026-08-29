#include <bits/stdc++.h>
using namespace std;

// reference to https://cs.adelaide.edu.au/~optlog/research/ttp/2015gecco-ttp.pdf

struct Packing {
    bool picked[MAX_ITEMS];
    long totalWeight = 0, totalProfit = 0;
};

struct PackParameter {
    int ptries = 20;
} packParams;

// itemsAtCity via CSR (sorted-by-city flat layout)
int itemsByCity[MAX_ITEMS];          // item indices, grouped by city
int cityItemStart[MAX_CITIES + 1];   // start offset per city (exclusive end at +1)
int cityIdToNodeIdx[MAX_CITIES];     // 1-based city ID -> 0-based node index

double nu;

void packInit() {
    log_debug("packing: initializing packing structures");
    log_debug("packing: nCities=", nCities, ", nItems=", nItems,
         ", capacity=", capacity, ", minSpeed=", minSpeed,
         ", maxSpeed=", maxSpeed);

    // build city-ID map
    for (long i = 0; i < nCities; ++i) {
        cityIdToNodeIdx[nodes[i].id] = (int)i;
    }

    // build itemsAtCity (CSR): count per city, prefix-sum, fill
    static int count[MAX_CITIES];
    for (long i = 0; i < nCities; ++i) count[i] = 0;
    for (long k = 0; k < nItems; ++k) {
        int nodeIdx = cityIdToNodeIdx[items[k].city];
        if (nodeIdx != -1) count[nodeIdx]++;
    }
    cityItemStart[0] = 0;
    for (long i = 0; i < nCities; ++i)
        cityItemStart[i + 1] = cityItemStart[i] + count[i];

    for (long i = 0; i < nCities; ++i) count[i] = cityItemStart[i];
    for (long k = 0; k < nItems; ++k) {
        int nodeIdx = cityIdToNodeIdx[items[k].city];
        if (nodeIdx != -1) itemsByCity[count[nodeIdx]++] = (int)k;
    }

    // --- speed / capacity constants for evaluate() ---
    nu = (maxSpeed - minSpeed) / capacity;   // ν = speed decay per unit weight
    log_debug("packing: initialized city map and item buckets");
}

// Incremental knapsack evaluation (O(m·n) per try, matching aco++ compute_fitness).
Packing pack(const int* tour, int tourLen) {
    Packing best;
    memset(best.picked, 0, nItems);


    static bool in_tour[MAX_CITIES];
    memset(in_tour, 0, nCities);
    for (int i = 0; i < tourLen; ++i) in_tour[tour[i]] = true;

    // distance_accumulated[city] = distance from tour start to the city's position
    static double distance_accumulated[MAX_CITIES];
    double total_distance = 0.0;
    for (int i = 0; i < tourLen - 1; ++i) {
        distance_accumulated[tour[i]] = total_distance;
        total_distance += distance(tour[i], tour[i + 1]);
    }
    distance_accumulated[tour[tourLen - 1]] = total_distance;
    
    // end_dist = accumulated distance at the true final city of the tour.
    // (not-simple-2 has no dummy node: tourLen-1 is the actual destination)
    const double end_dist = distance_accumulated[tour[tourLen - 1]];

    // PACKLOG (identical format to realbench/thop.c compute_fitness) — disabled for speed
#if 0
    static long pack_call = 0; ++pack_call;
    fprintf(stderr, "PACKLOG|call=%ld|tour_len=%ld|end_dist=%.0f\n",
            pack_call, (long)tourLen, end_dist);
    fprintf(stderr, "PACKLOG|tour|");
    for (int i = 0; i < tourLen; ++i) fprintf(stderr, "%ld,", (long)tour[i]);
    fprintf(stderr, "\n");
#endif

    static double score[MAX_ITEMS];
    static int order[MAX_ITEMS];
    static long profit_accumulated[MAX_CITIES];
    static long weight_accumulated[MAX_CITIES];
    static bool tmp_packing[MAX_ITEMS];

    long best_profit = 0;
    const double EPSILON = 1e-9; 

    for (int attempt = 0; attempt < packParams.ptries; attempt++) {
        // random θ δ γ
        double theta = ran01(&seed);
        double delta = ran01(&seed);
        double gamma = ran01(&seed);
        double sum = theta + delta + gamma;
        theta /= sum; delta /= sum; gamma /= sum;

#if 0
        fprintf(stderr, "PACKLOG|try=%ld|th=%.9f|de=%.9f|ga=%.9f\n",
                (long)attempt, theta, delta, gamma);
#endif

        // score items (negated ascending sort puts best first)
        memset(tmp_packing, 0, nItems);
        int valid_items_count = 0;
        for (int j = 0; j < nItems; ++j) {
            int node = cityIdToNodeIdx[items[j].city];
            if (!in_tour[node]) continue;
            
            double d_end = end_dist - distance_accumulated[node];
            d_end = max(d_end, 1e-12); // Tránh lỗi chia cho 0 trong C++
            score[j] = -pow((double)items[j].profit, theta) /
                       (pow((double)items[j].weight, delta) * pow(d_end, gamma));
            order[valid_items_count++] = j;
        }
        sort(order, order + valid_items_count, [&](int x, int y) { return score[x] < score[y]; });

        for (int i = 0; i < nCities; ++i) { profit_accumulated[i] = 0; weight_accumulated[i] = 0; }

        long total_weight = 0, total_profit = 0;

        for (int k = 0; k < valid_items_count; ++k) {
            int j = order[k];
            long w = items[j].weight;
            if (total_weight + w > capacity) continue;

            int node = cityIdToNodeIdx[items[j].city];
            profit_accumulated[node] += items[j].profit;
            weight_accumulated[node] += w;

            // O(n) time-feasibility sweep
            bool violate = false;
            
            long weight_so_far = 0; 
            
            double t_time = 0.0;
            int prev = 0; // C code gốc khởi tạo `prev_city = 0;` 
            
            for (int i = 1; i < tourLen; ++i) {
                int curr = tour[i];
                
                // [FIX 2] C skips empty stations except the last real city
                // (`instance.n - 2`), which sits just before the dummy. not-simple-2
                // has NO dummy, so its last real city / destination is `nCities - 1`.
                if (weight_accumulated[curr] == 0 && curr != (nCities - 1)) continue;

                double speed = maxSpeed - nu * weight_so_far;
                t_time += distance(prev, curr) / speed;

                if (t_time - EPSILON > maxTime) { violate = true; break; }
                
                weight_so_far += weight_accumulated[curr];
                prev = curr;
            }

            if (!violate) {
                total_profit += items[j].profit;
                total_weight += w;
                tmp_packing[j] = true;
            } else {
                profit_accumulated[node] -= items[j].profit;
                weight_accumulated[node] -= w;
            }
        }

        if (total_profit > best_profit) {
            best_profit = total_profit;
            memcpy(best.picked, tmp_packing, nItems);
        }

#if 0
        fprintf(stderr, "PACKLOG|try=%ld|profit=%ld|weight=%ld\n",
                (long)attempt, total_profit, total_weight);
#endif
    }

    best.totalProfit = best_profit;
    long tw = 0;
    for (int j = 0; j < nItems; ++j) if (best.picked[j]) tw += items[j].weight;
    best.totalWeight = tw;

#if 0
    fprintf(stderr, "PACKLOG|best|profit=%ld|weight=%ld\n",
            best.totalProfit, best.totalWeight);
#endif
    return best;
}
