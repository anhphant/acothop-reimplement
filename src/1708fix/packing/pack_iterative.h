#pragma once
#include <queue>

using namespace std;

static int visitedCitiesArr[MAX_CITIES];
static int pathArr[MAX_CITIES];
static bool alreadyPicked[MAX_ITEMS];

double computeTime(const int* tour, int tour_size, Packing& packing) {
    long weight = 0;
    double time = 0.0;

    int visited_len = 0;
    for (int i=0; i<tour_size; i++) {
        int city = tour[i];
        bool hasPicked = false;
        for (int k = 0; k < numItemsAtCity[city]; k++) {
            int item = itemsAtCityArr[city][k];
            if (packing.picked.test(item)) {
                hasPicked = true;
                break;
            }
        }
        if (hasPicked)
            visitedCitiesArr[visited_len++] = city;
    }

    int path_len = 0;
    pathArr[path_len++] = tour[0];
    for (int i=0; i<visited_len; i++)
        pathArr[path_len++] = visitedCitiesArr[i];
    if (pathArr[path_len - 1] != tour[tour_size - 1])
        pathArr[path_len++] = tour[tour_size - 1];

    for (int i=0; i<nItems; i++) alreadyPicked[i] = false;

    for (int i = 0; i + 1 < path_len; i++) {
        int city = pathArr[i];

        for (int k = 0; k < numItemsAtCity[city]; k++) {
            int item = itemsAtCityArr[city][k];
            if (!packing.picked.test(item)) continue;
            if (alreadyPicked[item]) continue;
            alreadyPicked[item] = true;

            weight += items[item].weight;
        }

        double speed = maxSpeed - nu * weight;
        if (speed < minSpeed)
            speed = minSpeed;

        time += distance(city, pathArr[i + 1]) / speed;
    }

    return time;
}

struct ItemNode {
    int item_idx;
    double score;
    bool operator<(const ItemNode& other) const {
        return score < other.score;
    }
};

Packing packIterative(const int* tour, int tour_size) {
    Packing cur;
    cur.picked.reset();
    cur.totalWeight = 0;
    cur.totalProfit = 0;

    priority_queue<ItemNode> pq;

    double baseTime = computeTime(tour, tour_size, cur);

    for (int i = 0; i < nItems; i++) {
        if (cur.totalWeight + items[i].weight > capacity) continue;
        cur.picked.set(i);
        double newTime = computeTime(tour, tour_size, cur);
        cur.picked.reset(i);

        double delta_t = newTime - baseTime;
        if (delta_t > 0 && newTime <= maxTime) {
            double s = (double)items[i].profit / delta_t;
            pq.push({i, s});
        }
    }

    while (!pq.empty()) {
        ItemNode top = pq.top();
        pq.pop();

        if (cur.picked.test(top.item_idx)) continue;
        if (cur.totalWeight + items[top.item_idx].weight > capacity) continue;

        cur.picked.set(top.item_idx);
        double newTime = computeTime(tour, tour_size, cur);
        cur.picked.reset(top.item_idx);

        double delta_t = newTime - baseTime;
        if (delta_t <= 0 || newTime > maxTime) continue;

        double s = (double)items[top.item_idx].profit / delta_t;

        bool is_best = true;
        if (!pq.empty()) {
            if (s < pq.top().score - 1e-6) {
                is_best = false;
            }
        }

        if (is_best) {
            cur.picked.set(top.item_idx);
            cur.totalWeight += items[top.item_idx].weight;
            cur.totalProfit += items[top.item_idx].profit;
            baseTime = newTime;

            int city = items[top.item_idx].city;
            for (int k = 0; k < numItemsAtCity[city]; k++) {
                int other_item = itemsAtCityArr[city][k];
                if (!cur.picked.test(other_item) && cur.totalWeight + items[other_item].weight <= capacity) {
                    cur.picked.set(other_item);
                    double nt = computeTime(tour, tour_size, cur);
                    cur.picked.reset(other_item);
                    double dt = nt - baseTime;
                    if (dt > 0 && nt <= maxTime) {
                        pq.push({other_item, (double)items[other_item].profit / dt});
                    }
                }
            }
        } else {
            pq.push({top.item_idx, s});
        }
    }

    return cur;
}

