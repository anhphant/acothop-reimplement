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

        time += packing_distance(city, path[i + 1]) / speed;
    }

    if (time > maxTime)
        return -INF;

    packing.totalWeight = weight;
    packing.totalProfit = profit;

    return profit;
}

Packing pack(const vector<int>& tour){
    Packing best;
    best.picked.assign(nItems, false);

    double bestProfit = -INF;

    mt19937 rng(rand());

    uniform_real_distribution<double> dist(0.0, 1.0);

    vector<double> score(nItems);

    for (int attempt = 0; attempt < packParams.ptries; attempt++)
    {
        //---------------------------------------
        // random θ δ γ
        //---------------------------------------

        double theta = dist(rng);
        double delta = dist(rng);
        double gamma = dist(rng);

        double sum = theta + delta + gamma;

        theta /= sum;
        delta /= sum;
        gamma /= sum;

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

        Packing cur;
        cur.picked.assign(nItems, false);

        for (int item : order)
        {
            cur.picked[item] = true;

            if (computeFitness(tour, cur) == -INF)
            {
                cur.picked[item] = false;
            }
        }

        double profit = computeFitness(tour, cur);

        if (profit > bestProfit)
        {
            bestProfit = profit;
            best = cur;
        }
    }

    return best;
}