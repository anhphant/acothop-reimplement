import sys

with open('packing/packing.cpp', 'r', encoding='utf-8', errors='ignore') as f:
    content = f.read()

import re
old_pattern = r"        // score items \([^\n]*\n        memset\(tmp_packing, 0, nItems\);\n        for \(int j = 0; j < nItems; \+\+j\) \{\n            int node = cityIdToNodeIdx\[items\[j\]\.city\];\n            double d_end = end_dist - distance_accumulated\[node\];\n            d_end = max\(d_end, 1e-12\);\n            score\[j\] = -pow\(\(double\)items\[j\]\.profit, theta\) /\n                       \(pow\(\(double\)items\[j\]\.weight, delta\) \* pow\(d_end, gamma\)\);\n            order\[j\] = j;\n        \}\n        sort\(order, order \+ nItems, \[&\]\(int x, int y\) \{ return score\[x\] < score\[y\]; \}\);\n\n        for \(int i = 0; i < nCities; \+\+i\) \{ profit_accumulated\[i\] = 0; weight_accumulated\[i\] = 0; \}\n\n        long total_weight = 0, total_profit = 0;\n\n        for \(int k = 0; k < nItems; \+\+k\) \{"

new_text = """        // score items (negated ascending sort puts best first)
        memset(tmp_packing, 0, nItems);
        int valid_items_count = 0;
        for (int j = 0; j < nItems; ++j) {
            int node = cityIdToNodeIdx[items[j].city];
            if (!in_tour[node]) continue;
            
            double d_end = end_dist - distance_accumulated[node];
            d_end = max(d_end, 1e-12);
            score[j] = -pow((double)items[j].profit, theta) /
                       (pow((double)items[j].weight, delta) * pow(d_end, gamma));
            order[valid_items_count++] = j;
        }
        sort(order, order + valid_items_count, [&](int x, int y) { return score[x] < score[y]; });

        for (int i = 0; i < nCities; ++i) { profit_accumulated[i] = 0; weight_accumulated[i] = 0; }

        long total_weight = 0, total_profit = 0;

        for (int k = 0; k < valid_items_count; ++k) {"""

new_content = re.sub(old_pattern, new_text, content, flags=re.DOTALL)
if new_content == content:
    print("PATCH FAILED!")
else:
    with open('packing/packing.cpp', 'w', encoding='utf-8') as f:
        f.write(new_content)
    print("PATCH SUCCESS!")
