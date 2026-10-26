#include <bits/stdc++.h>
using namespace std;
#include "helper/helper.cpp"
#include "problem/thopproblem.cpp"
int main() {
    ifstream fin("../../instances/dsj1000-thop/dsj1000_10_usw_10_03.thop");
    string line;
    auto skip = [&] { getline(fin, line); };
    auto take_after_prefix = [&](size_t prefix) { return line.size() > prefix ? line.substr(prefix) : string{}; };
    skip(); name = take_after_prefix(14);
    skip(); knapsackType = take_after_prefix(19);
    fin.ignore(256, ':'); fin >> nCities;
    fin.ignore(256, ':'); fin >> nItems;
    fin.ignore(256, ':'); fin >> capacity;
    fin.ignore(256, ':'); fin >> maxTime;
    fin.ignore(256, ':'); fin >> minSpeed;
    fin.ignore(256, ':'); fin >> maxSpeed;
    fin.ignore(256, '\n');
    skip(); edgeWeightType = line.substr(17);
    skip();                      // NODE_COORD_SECTION
    for (long i = 0; i < nCities; ++i) {
        fin >> nodes[i].id >> nodes[i].x >> nodes[i].y;
        nodes[i].id--;
    }
    cout << "nCities: " << nCities << endl;
    cout << "edgeWeightType: " << edgeWeightType << endl;
    cout << "nodes[0]: " << nodes[0].id << " " << nodes[0].x << " " << nodes[0].y << endl;
    cout << "nodes[913]: " << nodes[913].id << " " << nodes[913].x << " " << nodes[913].y << endl;
    cout << "nodes[816]: " << nodes[816].id << " " << nodes[816].x << " " << nodes[816].y << endl;
    precalculate_distances();
    cout << "dist(0, 913) = " << distance(0, 913) << endl;
    cout << "dist(0, 816) = " << distance(0, 816) << endl;
    
    int cur = 0;
    bool visited[1000] = {0};
    visited[0] = true;
    int next = -1;
    double best = numeric_limits<double>::infinity();
    for(int j=0; j<nCities; j++) {
        if (visited[j] || j == nCities - 1) continue;
        double d = distance(cur, j);
        if (d < best) { best = d; next = j; }
    }
    cout << "NN of 0: " << next << " with distance " << best << endl;
}
