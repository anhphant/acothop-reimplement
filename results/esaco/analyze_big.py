import re, glob, statistics
from collections import defaultdict

pat = re.compile(r'^(\w+)_(\d+)_(\w+)_(\w+)_(\d+)_(aco|origin)_seed(\d+)(\.sol)?\.log$')

def parse(f):
    txt = open(f, encoding='utf-8', errors='replace').read()
    vals = [int(x) for x in re.findall(r'best\s+(\d+),', txt)]
    vals += [int(x) for x in re.findall(r'cost\s+(\d+)', txt)]
    m2 = re.search(r'Best objective:\s*([\d.]+)', txt)
    if m2: vals.append(int(float(m2.group(1))))
    complete = bool(m2) or bool(re.search(r'try\s+\d+,\s+best\s+\d+', txt))
    return (max(vals) if vals else None), complete

cfg = defaultdict(lambda: defaultdict(list))
inc = defaultdict(int); tot = defaultdict(int)
for f in [x for x in glob.glob('*.log') + glob.glob('*.sol.log')
          if x.startswith(('d15112_', 'pla33810_'))]:
    m = pat.match(f)
    if not m: continue
    inst, grp, typ, cap, idx, algo = m.groups()[:6]
    val, complete = parse(f)
    tot[(inst, algo)] += 1
    if not complete: inc[(inst, algo)] += 1
    if val is not None:
        cfg[(inst, grp, typ, cap)][algo].append(val)

print('COMPLETION:')
for k in sorted(tot):
    print(f'  {k[0]} {k[1]:7s}: complete {tot[k]-inc[k]}/{tot[k]}, partial/incomplete {inc[k]}')
print()

hdr = f"{'instance':<10}{'grp':<4}{'type':<5}{'cap':<4}{'nA':<4}{'mean_aco':<12}{'nO':<4}{'mean_origin':<13}{'diff%':<8}{'winner'}"
print(hdr)
print('-' * len(hdr))
wins = {'aco': 0, 'origin': 0, 'tie': 0}
for k in sorted(cfg):
    a, o = cfg[k]['aco'], cfg[k]['origin']
    if not a or not o: continue
    ma, mo = statistics.mean(a), statistics.mean(o)
    d = (ma - mo) / mo * 100
    w = 'aco' if ma > mo else ('origin' if mo > ma else 'tie')
    wins[w] += 1
    print(f"{k[0]:<10}{k[1]:<4}{k[2]:<5}{k[3]:<4}{len(a):<4}{ma:<12.1f}{len(o):<4}{mo:<13.1f}{d:<8.2f}{w}")
print(f'Total comparable configs: {sum(wins.values())} | wins: aco={wins["aco"]}, origin={wins["origin"]}, tie={wins["tie"]}')
print('NOTE: mean of best-so-far values; incomplete runs may understate final quality.')
