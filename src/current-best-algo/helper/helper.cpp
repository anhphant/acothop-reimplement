#include <bits/stdc++.h>
using namespace std;

unsigned int seed = 1910;
mt19937 rng(seed);
uniform_real_distribution<double> rand_prob(0.0, 1.0);
chrono::steady_clock::time_point start_time = chrono::steady_clock::now();

const long double INF = numeric_limits<long double>::infinity();
const long double epsilon = 1e-10;



double elapsed_time()  {
    auto now = chrono::steady_clock::now();
    return chrono::duration<double>(now - start_time).count();
}

string log_timestamp() {
    ostringstream oss;
    oss << fixed << setprecision(3) << elapsed_time() << "s";
    return oss.str();
}

template <typename... Args>
void log_debug(Args&&... args) {
    cerr << "[" << log_timestamp() << "] ";
    (cerr << ... << args);
    cerr << '\n';
}