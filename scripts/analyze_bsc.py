import re

with open('ns_bsc.log') as f: ns = f.readlines()
with open('rb_bsc.log') as f: rb = f.readlines()
ns_pack = [l for l in ns if l.startswith('PACKLOG')]
rb_pack = [l for l in rb if l.startswith('PACKLOG')]

# NN tour (call=1)
ns_call1 = [l for l in ns_pack if 'call=1|' in l]
rb_call1 = [l for l in rb_pack if 'call=1|' in l]
print('NS call=1:', ns_call1[0].strip() if ns_call1 else 'NOT FOUND')
print('RB call=1:', rb_call1[0].strip() if rb_call1 else 'NOT FOUND')

# Total pack calls
ns_calls = [l for l in ns_pack if re.match(r'PACKLOG\|call=\d+\|', l)]
rb_calls = [l for l in rb_pack if re.match(r'PACKLOG\|call=\d+\|', l)]
print(f'\nNS total pack calls: {len(ns_calls)}')
print(f'RB total pack calls: {len(rb_calls)}')

# Best profits
ns_best = [l for l in ns_pack if l.startswith('PACKLOG|best|')]
rb_best = [l for l in rb_pack if l.startswith('PACKLOG|best|')]

ns_profits = []
for l in ns_best:
    m = re.search(r'profit=(\d+)', l)
    if m: ns_profits.append(int(m.group(1)))
rb_profits = []
for l in rb_best:
    m = re.search(r'profit=(\d+)', l)
    if m: rb_profits.append(int(m.group(1)))

print(f'\nNS best profits: min={min(ns_profits)}, max={max(ns_profits)}, last={ns_profits[-1]}')
print(f'RB best profits: min={min(rb_profits)}, max={max(rb_profits)}, last={rb_profits[-1]}')

print('\nNS best profits (first 20):', ns_profits[:20])
print('RB best profits (first 20):', rb_profits[:20])

# Tour length comparison (call=2 onwards - these are ACO tours after LS)
ns_tour_lens = []
rb_tour_lens = []
for l in ns_calls[1:11]:  # skip call=1 (NN), show next 10
    m = re.search(r'tour_len=(\d+)', l)
    if m: ns_tour_lens.append(int(m.group(1)))
for l in rb_calls[1:11]:
    m = re.search(r'tour_len=(\d+)', l)
    if m: rb_tour_lens.append(int(m.group(1)))

print('\nNS tour lengths (calls 2-11):', ns_tour_lens)
print('RB tour lengths (calls 2-11):', rb_tour_lens)
