import re, glob, os, statistics
from collections import defaultdict

pat_log = re.compile(r'^(\w+)_(\d+)_(\w+)_(\w+)_(\d+)_(aco|origin)_seed(\d+)\.log$')
pat_sol = re.compile(r'^(\w+)_(\d+)_(\w+)_(\w+)_(\d+)_(aco|origin)_seed(\d+)\.sol\.log$')

rows = {}
for f in glob.glob('*.log') + glob.glob('*.sol.log'):
    m = pat_log.match(f) or pat_sol.match(f)
    if not m: continue
    inst, grp, typ, cap, idx, algo, seed = m.groups()
    key = (inst, grp, typ, cap, idx, algo, seed)
    txt = open(f, encoding='utf-8', errors='replace').read()
    if f.endswith('.sol.log'):
        bests = re.findall(r'try\s+\d+,\s+best\s+(\d+)', txt)
        val = max(int(b) for b in bests) if bests else None
    else:
        m2 = re.search(r'Best objective:\s*([\d.]+)', txt)
        val = int(float(m2.group(1))) if m2 else None
    if key not in rows or (val is not None and (rows[key] is None or val > rows[key])):
        rows[key] = val

print("Total parsed runs:", len(rows), "| complete:", sum(1 for v in rows.values() if v is not None), "| incomplete:", sum(1 for v in rows.values() if v is None))

with open('_analysis_raw.csv', 'w') as out:
    out.write("instance,group,type,cap,idx,algo,seed,best\n")
    for (inst, grp, typ, cap, idx, algo, seed), v in sorted(rows.items()):
        out.write(f"{inst},{grp},{typ},{cap},{idx},{algo},{seed},{v if v is not None else ''}\n")

cfg = defaultdict(lambda: defaultdict(list))
for (inst, grp, typ, cap, idx, algo, seed), v in rows.items():
    if v is None: continue
    cfg[(inst, grp, typ, cap)][algo].append(v)

print(f"\n{'instance':<10}{'grp':<4}{'type':<5}{'cap':<4}{'nA':<4}{'mean_aco':<12}{'nO':<4}{'mean_origin':<13}{'diff%':<8}{'winner'}")
print('-'*75)
wins = {'aco':0, 'origin':0, 'tie':0}
for k in sorted(cfg):
    a = cfg[k]['aco']; o = cfg[k]['origin']
    if not a or not o: continue
    ma, mo = statistics.mean(a), statistics.mean(o)
    d = (ma-mo)/mo*100
    w = 'aco' if ma > mo else ('origin' if mo > ma else 'tie')
    wins[w] += 1
    print(f"{k[0]:<10}{k[1]:<4}{k[2]:<5}{k[3]:<4}{len(a):<4}{ma:<12.1f}{len(o):<4}{mo:<13.1f}{d:<8.2f}{w}")

print(f"\nWins: ACO={wins['aco']}, ORIGIN={wins['origin']}, TIE={wins['tie']} (of {sum(wins.values())} configs)")

# by group
for grp in ['01','03','05','10']:
    sub = {k:v for k,v in cfg.items() if k[1]==grp}
    for algo in ['aco','origin']:
        vals = [statistics.mean(v[algo]) for v in sub.values() if v[algo]]
        if vals: print(f"group {grp} {algo}: mean-of-means = {statistics.mean(vals):.1f} ({len(vals)} configs)")

# paired comparison on same seed
paired = defaultdict(list)
for key, v in rows.items():
    inst, grp, typ, cap, idx, algo, seed = key
    if v is None: continue
    paired[(inst, grp, typ, cap, idx, seed)].append((algo, v))
diffs = []
for k, lst in paired.items():
    d = {a: v for a, v in lst}
    if 'aco' in d and 'origin' in d:
        diffs.append((d['aco'] - d['origin']) / d['origin'] * 100)
if diffs:
    print(f"\nPaired same-seed: n={len(diffs)}, mean diff (aco-origin) = {statistics.mean(diffs):+.2f}%, "
          f"median = {statistics.median(diffs):+.2f}%, "
          f"aco better: {sum(1 for x in diffs if x>0)}, origin better: {sum(1 for x in diffs if x<0)}, equal: {sum(1 for x in diffs if x==0)}")
