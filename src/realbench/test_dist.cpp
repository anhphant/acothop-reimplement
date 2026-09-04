#include <iostream>
#include <cmath>
#include <vector>
#include <fstream>
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
    cout << "dist(0, 913) = " << dist(0, 913) << endl;
    cout << "dist(0, 816) = " << dist(0, 816) << endl;
}
