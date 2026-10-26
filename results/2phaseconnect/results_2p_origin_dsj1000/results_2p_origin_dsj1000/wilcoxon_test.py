import csv
import numpy as np
from scipy.stats import wilcoxon

rows = list(csv.DictReader(open('head_to_head.csv')))
origin = np.array([float(r['origin_mean']) for r in rows])
two_p = np.array([float(r['2p_mean']) for r in rows])
d = two_p - origin

print(f'n = {len(d)} instances')
print(f'mean origin = {origin.mean():.3f}')
print(f'mean 2p     = {two_p.mean():.3f}')
print(f'mean diff (2p - origin) = {d.mean():.3f}')
print(f'wins 2p / origin / ties = {(d > 0).sum()} / {(d < 0).sum()} / {(d == 0).sum()}')

res = wilcoxon(two_p, origin, zero_method='wilcox', alternative='two-sided')
print(f'\nWilcoxon signed-rank (two-sided): W = {res.statistic:.1f}, p = {res.pvalue:.6g}')

res_g = wilcoxon(two_p, origin, zero_method='wilcox', alternative='greater')
print(f'one-sided (2p > origin): p = {res_g.pvalue:.6g}')
