#include <iostream>
#include <cmath>
#include <vector>
#include <fstream>
#include <algorithm>
using namespace std;
struct Point { double x, y; };
int main() {
    ifstream fin("../../instances/dsj1000-thop/dsj1000_10_usw_10_03.thop");
    string s;
    while(fin >> s && s != "Y):") {}
    vector<Point> p(1000);
    for(int i=0; i<1000; i++) {
        int id; fin >> id >> p[i].x >> p[i].y;
    }
    auto dist = [&](int i, int j) {
        double dx = p[i].x - p[j].x;
        double dy = p[i].y - p[j].y;
        return (int)ceil(sqrt(dx*dx + dy*dy));
    };
    
    // Compute nn for city 0
    vector<pair<int, int>> nn;
    for(int j=0; j<1000; j++) {
        if(j != 0) nn.push_back({dist(0, j), j});
    }
    sort(nn.begin(), nn.end());
    cout << "knn for 0: ";
    for(int i=0; i<20; i++) cout << nn[i].second << " ";
    cout << endl;
    
    // Check if 999 is in knn of 0
    for(int i=0; i<20; i++) {
        if (nn[i].second == 999) cout << "999 is in knn of 0!" << endl;
    }
}
