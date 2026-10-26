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

struct ItemScore {
    double score;
    int idx;
    bool operator<(const ItemScore& other) const {
        return score < other.score;
    }
};

// Static working buffers for O(N) pack
static bool in_tour[MAX_CITIES];
static int tour_pos[MAX_CITIES];
static double distance_accumulated[MAX_CITIES];
static double pow_d_end[MAX_CITIES];
static int dist_to_dest[MAX_CITIES];
static int tour_items[MAX_ITEMS];
static ItemScore item_scores[MAX_ITEMS];
static long weight_accumulated[MAX_CITIES];
static bool tmp_packing[MAX_ITEMS];
static int picked_items[MAX_ITEMS];
static int active_indices[MAX_CITIES];
static int station_active_pos[MAX_CITIES];
static double prefix_time[MAX_CITIES];
static long prefix_weight[MAX_CITIES];
static double temp_prefix_time[MAX_CITIES];
static long temp_prefix_weight[MAX_CITIES];

// O(N) Knapsack packing evaluation with bit-for-bit equivalence to original.
Packing pack(const int* tour, int tourLen) {
    Packing best;
    memset(best.picked, 0, nItems);

    int dest_node = tour[tourLen - 1];
    for (int i = 0; i < tourLen; ++i) {
        int u = tour[i];
        in_tour[u] = true;
        tour_pos[u] = i;
        dist_to_dest[u] = distance(u, dest_node);
    }

    double total_distance = 0.0;
    for (int i = 0; i < tourLen - 1; ++i) {
        distance_accumulated[tour[i]] = total_distance;
        total_distance += distance(tour[i], tour[i + 1]);
    }
    distance_accumulated[tour[tourLen - 1]] = total_distance;
    const double end_dist = total_distance;

    // Tour-Items Pre-filtering: filter once per pack() in increasing j order
    int tour_items_count = 0;
    for (int j = 0; j < nItems; ++j) {
        int node = cityIdToNodeIdx[items[j].city];
        if (in_tour[node]) {
            tour_items[tour_items_count++] = j;
        }
    }

    long best_profit = 0;
    const double EPSILON = 1e-9; 
    const double inv_vmax = 1.0 / maxSpeed;
    const double inv_vmax_sq = nu / (maxSpeed * maxSpeed);

    for (int attempt = 0; attempt < packParams.ptries; attempt++) {
        // random θ δ γ (identical RNG sequence)
        double theta = ran01(&seed);
        double delta = ran01(&seed);
        double gamma = ran01(&seed);
        double sum = theta + delta + gamma;
        theta /= sum; delta /= sum; gamma /= sum;

        // Precompute pow(d_end, gamma) once per tour city
        for (int i = 0; i < tourLen; ++i) {
            int u = tour[i];
            double d_end = max(end_dist - distance_accumulated[u], 1e-12);
            pow_d_end[u] = pow(d_end, gamma);
        }

        int valid_items_count = tour_items_count;
        for (int it = 0; it < tour_items_count; ++it) {
            int j = tour_items[it];
            int node = cityIdToNodeIdx[items[j].city];
            double s = -pow((double)items[j].profit, theta) /
                       (pow((double)items[j].weight, delta) * pow_d_end[node]);
            item_scores[it] = {s, j};
        }
        sort(item_scores, item_scores + valid_items_count);

        long total_weight = 0, total_profit = 0;
        int num_picked = 0;

        // Active stations tracking
        int num_active = 1;
        active_indices[0] = tourLen - 1;
        station_active_pos[dest_node] = 0;
        prefix_time[0] = distance(0, dest_node) * inv_vmax;
        prefix_weight[0] = 0;
        double current_total_time = prefix_time[0];

        for (int k = 0; k < valid_items_count; ++k) {
            int j = item_scores[k].idx;
            long w = items[j].weight;
            if (total_weight + w > capacity) continue;

            int node = cityIdToNodeIdx[items[j].city];
            int p = tour_pos[node];
            double slack = maxTime - current_total_time;

            bool is_new_station = (weight_accumulated[node] == 0 && node != dest_node);
            int insert_pos = 0;

            if (is_new_station) {
                while (insert_pos < num_active - 1 && active_indices[insert_pos] < p) {
                    insert_pos++;
                }

                // O(1) Physical Lower-Bound Pruning
                int prev_st = (insert_pos == 0) ? 0 : tour[active_indices[insert_pos - 1]];
                int next_st = tour[active_indices[insert_pos]];
                int geom_detour = distance(prev_st, node) + distance(node, next_st) - distance(prev_st, next_st);
                int dist_dest = dist_to_dest[node];
                double min_dt = geom_detour * inv_vmax + (w * dist_dest) * inv_vmax_sq;
                if (min_dt > slack + EPSILON) continue; // O(1) PRUNE!

                for (int s = num_active; s > insert_pos; --s) {
                    active_indices[s] = active_indices[s - 1];
                    station_active_pos[tour[active_indices[s]]] = s;
                }
                active_indices[insert_pos] = p;
                station_active_pos[node] = insert_pos;
                num_active++;
            } else {
                insert_pos = station_active_pos[node];
                int dist_dest = dist_to_dest[node];
                double min_dt = (w * dist_dest) * inv_vmax_sq;
                if (min_dt > slack + EPSILON) continue; // O(1) PRUNE!
            }

            weight_accumulated[node] += w;

            // Prefix Time & Weight Caching (Suffix Sweep)
            bool violate = false;
            double t_time;
            long weight_so_far;
            int prev;
            int start_s;

            if (is_new_station) {
                if (insert_pos == 0) {
                    prev = 0;
                    t_time = 0.0;
                    weight_so_far = 0;
                } else {
                    prev = tour[active_indices[insert_pos - 1]];
                    t_time = prefix_time[insert_pos - 1];
                    weight_so_far = prefix_weight[insert_pos - 1];
                }
                start_s = insert_pos;
            } else {
                prev = node;
                t_time = prefix_time[insert_pos];
                weight_so_far = prefix_weight[insert_pos] + w;
                start_s = insert_pos + 1;
            }

            for (int s = start_s; s < num_active; ++s) {
                int curr = tour[active_indices[s]];
                double speed = maxSpeed - nu * weight_so_far;
                t_time += distance(prev, curr) / speed;
                if (t_time - EPSILON > maxTime) { violate = true; break; }
                weight_so_far += weight_accumulated[curr];
                temp_prefix_time[s] = t_time;
                temp_prefix_weight[s] = weight_so_far;
                prev = curr;
            }

            if (!violate) {
                total_profit += items[j].profit;
                total_weight += w;
                tmp_packing[j] = true;
                picked_items[num_picked++] = j;
                current_total_time = t_time;
                if (!is_new_station) {
                    prefix_weight[insert_pos] += w;
                }
                for (int s = start_s; s < num_active; ++s) {
                    prefix_time[s] = temp_prefix_time[s];
                    prefix_weight[s] = temp_prefix_weight[s];
                }
            } else {
                weight_accumulated[node] -= w;
                if (is_new_station) {
                    for (int s = insert_pos; s < num_active - 1; ++s) {
                        active_indices[s] = active_indices[s + 1];
                        station_active_pos[tour[active_indices[s]]] = s;
                    }
                    num_active--;
                }
            }
        }

        if (total_profit > best_profit) {
            best_profit = total_profit;
            memcpy(best.picked, tmp_packing, nItems);
        }

        // Sparse reset
        for (int p = 0; p < num_picked; ++p) {
            tmp_packing[picked_items[p]] = false;
        }
        for (int s = 0; s < num_active; ++s) {
            weight_accumulated[tour[active_indices[s]]] = 0;
        }
    }

    // Reset in_tour
    for (int i = 0; i < tourLen; ++i) in_tour[tour[i]] = false;

    best.totalProfit = best_profit;
    long tw = 0;
    for (int j = 0; j < nItems; ++j) if (best.picked[j]) tw += items[j].weight;
    best.totalWeight = tw;
    return best;
}
