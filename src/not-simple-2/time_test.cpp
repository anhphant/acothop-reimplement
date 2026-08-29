#include <iostream>
#include <chrono>
#include <cmath>
using namespace std;
int main() {
    auto start = chrono::high_resolution_clock::now();
    double sum = 0;
    for (int i = 0; i < 600000; i++) {
        sum += pow(i * 1.1, 0.4) / (pow(i * 0.5 + 1, 0.2) * pow(i * 0.1 + 1, 0.7));
    }
    auto end = chrono::high_resolution_clock::now();
    cout << "Time: " << chrono::duration_cast<chrono::milliseconds>(end - start).count() << "ms, sum=" << sum << endl;
    return 0;
}
