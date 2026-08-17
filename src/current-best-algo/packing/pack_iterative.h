#pragma once
#include <queue>
#include <vector>

using namespace std;

double computeTime(const vector<int>& tour, Packing& packing) {
    long weight = 0;
    double time = 0.0;

    vector<int> visitedCities;
    for (int city : tour) {
        bool hasPicked = false;
        for (int item : itemsAtCity[city]) {
            if (packing.picked[item]) {
                hasPicked = true;
                break;
            }
        }
        if (hasPicked)
            visitedCities.push_back(city);
    }

    vector<int> path;
    path.push_back(tour.front());
    for (int c : visitedCities)
        path.push_back(c);
    if (path.back() != tour.back())
        path.push_back(tour.back());

    vector<bool> alreadyPicked(nItems, false);

    for (size_t i = 0; i + 1 < path.size(); i++) {
        int city = path[i];

        for (int item : itemsAtCity[city]) {
            if (!packing.picked[item]) continue;
            if (alreadyPicked[item]) continue;
            alreadyPicked[item] = true;

            weight += items[item].weight;
        }

        double speed = maxSpeed - nu * weight;
        if (speed < minSpeed)
            speed = minSpeed;

        time += distance(city, path[i + 1]) / speed;
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

Packing packIterative(const vector<int>& tour) {
    Packing cur;
    cur.picked.assign(nItems, false);
    cur.totalWeight = 0;
    cur.totalProfit = 0;

    priority_queue<ItemNode> pq;

    double baseTime = computeTime(tour, cur);

    for (int i = 0; i < nItems; i++) {
        if (cur.totalWeight + items[i].weight > capacity) continue;
        cur.picked[i] = true;
        double newTime = computeTime(tour, cur);
        cur.picked[i] = false;

        double delta_t = newTime - baseTime;
        if (delta_t > 0 && newTime <= maxTime) {
            double s = (double)items[i].profit / delta_t;
            pq.push({i, s});
        }
    }

    while (!pq.empty()) {
        ItemNode top = pq.top();
        pq.pop();

        if (cur.picked[top.item_idx]) continue;
        if (cur.totalWeight + items[top.item_idx].weight > capacity) continue;

        cur.picked[top.item_idx] = true;
        double newTime = computeTime(tour, cur);
        cur.picked[top.item_idx] = false;

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
            cur.picked[top.item_idx] = true;
            cur.totalWeight += items[top.item_idx].weight;
            cur.totalProfit += items[top.item_idx].profit;
            baseTime = newTime;

            int city = items[top.item_idx].city;
            for (int other_item : itemsAtCity[city]) {
                if (!cur.picked[other_item] && cur.totalWeight + items[other_item].weight <= capacity) {
                    cur.picked[other_item] = true;
                    double nt = computeTime(tour, cur);
                    cur.picked[other_item] = false;
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
