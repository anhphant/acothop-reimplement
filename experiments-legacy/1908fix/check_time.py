import math
import sys

with open('../../instances/dsj1000-thop/dsj1000_10_usw_10_03.thop') as f:
    lines = f.readlines()

vertices = {}
for l in lines[10:1010]:
    parts = l.split()
    vertices[int(parts[0])] = (float(parts[1]), float(parts[2]))

items = {}
for l in lines[1011:1011+9980]:
    parts = l.split()
    items[int(parts[0])] = (float(parts[1]), float(parts[2]), int(parts[3]))

with open('output.txt') as f:
    out = f.readlines()

pt_str = out[0].replace(',', ' ').replace('[', '').replace(']', '')
pt = [1] + [int(x) for x in pt_str.split()] + [1000]

ci_str = out[1].replace(',', ' ').replace('[', '').replace(']', '')
ci = [int(x) for x in ci_str.split()]

aw = [0]*len(pt)
ap = [0]*len(pt)
max_speed = 1.0
v = 0.9/9122564

for j in range(len(ci)):
    item_node = items[ci[j]][2]
    # find where this node is in the tour
    # actually, THOP allows picking items only when you visit the node
    for i in range(len(pt)):
        if item_node == pt[i]:
            aw[i] += items[ci[j]][1]
            ap[i] += items[ci[j]][0]
            break

prev = 1
cw = 0
ct = 0.0
cd = 0.0

for i in range(1, len(pt)):
    dx = vertices[prev][0] - vertices[pt[i]][0]
    dy = vertices[prev][1] - vertices[pt[i]][1]
    d = math.ceil(math.sqrt(dx**2 + dy**2))
    ct += d / (max_speed - v * cw)
    cw += aw[i]
    cd += d
    prev = pt[i]

print(f"Python dist: {cd}, Python time: {ct}")
print(f"Number of picked items: {len(ci)}")
