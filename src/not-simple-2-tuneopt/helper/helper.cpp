#include <bits/stdc++.h>
using namespace std;

// Fixed capacity for static arrays. This is a 32-bit toolchain, so the three
// n×n `double` matrices (pheromone/total/dist_memo) cap us around ~4000
// cities before the 2 GB address space runs out. d2103 (2103 cities) is the
// largest instance that completes here.
constexpr int MAX_CITIES = 4000;
constexpr int MAX_ITEMS = 100000;
constexpr int MAX_ANTS = 1000;
constexpr int MAX_KN = 20;

#define IA 16807
#define IM 2147483647
#define AM (1.0/IM)
#define IQ 127773
#define IR 2836

long seed = 1910;

double ran01(long *idum) {
    long k;
    double ans;

    k = (*idum)/IQ;
    long temp = IA * (*idum - k * IQ) - IR * k;
    if (temp < 0 ) temp += IM;
    *idum = temp;
    ans = AM * (*idum);
    return ans;
}

chrono::steady_clock::time_point start_time = chrono::steady_clock::now();

const double INF = numeric_limits<double>::infinity();
const double epsilon = 1e-10;

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
