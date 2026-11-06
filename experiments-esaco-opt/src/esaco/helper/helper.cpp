#include <bits/stdc++.h>
using namespace std;

// Fixed capacity for static (1-D) arrays. The n x n distance/total/heuristic
// matrices are computed on the fly; phe is a lazy sparse store (uniform base
// value + touched-arc overlay), so memory no longer scales with n^2. Caps are
// driven by per-city/per-item buffers: 35000 covers pla33810 cities, 400000
// covers pla33810_10 338080 items.
constexpr int MAX_CITIES = 35000;
constexpr int MAX_ITEMS = 400000;
constexpr int MAX_ANTS = 500;
constexpr int MAX_KN = 100;
constexpr int MAX_KS = 10;   // ESACO S3: max size of the per-node sparse pheromone list

#define IA 16807
#define IM 2147483647
#define AM (1.0/IM)
#define IQ 127773
#define IR 2836

long seed = 1910;
FILE* log_report_file = nullptr;

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
