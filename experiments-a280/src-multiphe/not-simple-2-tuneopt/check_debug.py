import sys

with open('output.txt', 'r') as f:
    lines = f.readlines()

tour_line = lines[0].strip().split()
tour = [int(x) for x in tour_line]

items_line = lines[1].strip().split()
items_picked = [int(x) for x in items_line]

# read instance
cities = {}
items_data = {}
with open('..\..\instances\dsj1000-thop\dsj1000_10_usw_10_03.thop', 'r') as f:
    in_items = False
    for line in f:
        line = line.strip()
        if line.startswith('NODE_COORD_SECTION'):
            continue
        elif line.startswith('ITEMS SECTION'):
            in_items = True
            continue
        if not line:
            continue
        parts = line.split()
        if len(parts) >= 3 and not in_items:
            try:
                cities[int(parts[0])] = (float(parts[1]), float(parts[2]))
            except:
                pass
        elif len(parts) >= 4 and in_items:
            items_data[int(parts[0])] = {'p': int(parts[1]), 'w': int(parts[2]), 'c': int(parts[3])}

visited = set(tour)
for item_idx in items_picked:
    # 0-indexed or 1-indexed? items_picked in output.txt usually 1-indexed?
    c = items_data[item_idx]['c']
    if c not in visited:
        print(f'Item {item_idx} belongs to city {c} which is NOT in tour. Tour length: {len(tour)}')
        break
