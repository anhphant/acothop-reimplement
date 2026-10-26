#include <iostream>
#include <fstream>
#include <vector>
#include <string>

using namespace std;

struct Item { int id, profit, weight, city; };
Item items[10000];

int main() {
    ifstream fin(R"(..\..\instances\dsj1000-thop\dsj1000_10_usw_10_03.thop)");
    string s;
    for(int i=0;i<1011;i++) getline(fin, s);
    int p1, p2, p3, p4;
    while(fin >> p1 >> p2 >> p3 >> p4) {
        items[p1].id = p1;
        items[p1].profit = p2;
    }
    
    ifstream fsol("out.sol");
    getline(fsol, s); // tour
    getline(fsol, s); // items
    
    long long sum = 0;
    int current_num = 0;
    for(char c : s) {
        if(isdigit(c)) {
            current_num = current_num * 10 + (c - '0');
        } else if (c == ',' || c == ']') {
            if(current_num > 0) {
                sum += items[current_num].profit;
                current_num = 0;
            }
        }
    }
    cout << "C++ sum: " << sum << endl;
    return 0;
}
