#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>
using namespace std;
int main() {
    auto start = chrono::high_resolution_clock::now();
    long sum = 0;
    long weight_accumulated[1000] = {0};
    weight_accumulated[500] = 10;
    int tour[1000];
    for (int i=0; i<1000; i++) tour[i] = i;
    for (int ptries = 0; ptries < 2; ptries++) {
        for (int k = 0; k < 10000; ++k) {
            double t_time = 0;
            int prev = tour[0];
            for (int i = 1; i < 1000; ++i) {
                int curr = tour[i];
                if (weight_accumulated[curr] == 0 && curr != 999) continue;
                double speed = 1.0 - 0.001 * 0;
                t_time += (prev - curr) / speed;
                prev = curr;
            }
            if (t_time < 0) sum++;
        }
    }
    auto end = chrono::high_resolution_clock::now();
    cout << "Time: " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << "ms" << endl;
    return 0;
}
