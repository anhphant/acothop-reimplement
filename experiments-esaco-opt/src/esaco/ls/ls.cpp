// Faithful port of aco++ (realbench) ls.c into not-simple-2.
// Only substitutions: instance.n -> LS_N, instance.distance -> dist_memo.
// Original copyright Thomas Stuetzle (GPL) — see realbench/ls.c.

#include <bits/stdc++.h>
using namespace std;

#define LS_TRUE  1
#define LS_FALSE 0
#define LS_MAX(x,y) ((x)>=(y)?(x):(y))
#define LS_MIN(x,y) ((x)<=(y)?(x):(y))
#define LS_TRACE(x)
#define LS_DEBUG(x)

long int nn_ls   = 20;   /* max depth of NN lists used in the local search */
long int nn_ants = 20;   /* number of nearest neighbours in tour construction */
long int dlb_flag = LS_TRUE; /* use don't look bits */

static long int LS_N;    /* == nCities + 1 (realbench instance.n, includes dummy) */

void ls_init() {
    LS_N = (long int)nCities + 1;
}

/* --- swap2 / sort2 (realbench utilities.c) --- */
static void ls_swap2(long int *v, long int *v2, long int i, long int j) {
    long int tmp;
    tmp = v[i]; v[i] = v[j]; v[j] = tmp;
    tmp = v2[i]; v2[i] = v2[j]; v2[j] = tmp;
}

static void ls_sort2(long int *v, long int *v2, long int left, long int right) {
    long int k, last;
    if (left >= right) return;
    ls_swap2(v, v2, left, (left + right) / 2);
    last = left;
    for (k = left + 1; k <= right; k++)
        if (v[k] < v[left]) ls_swap2(v, v2, ++last, k);
    ls_swap2(v, v2, left, last);
    ls_sort2(v, v2, left, last);
    ls_sort2(v, v2, last + 1, right);
}

/* --- rotate_tour (realbench ls.c) --- */
static void rotate_tour(long int *tour, long int n) {
    int pos_0, i, j, k, ini, fin, tmp;
    for (pos_0 = 1; pos_0 < n; pos_0++) if (tour[pos_0] == 0) break;

    if (tour[pos_0 - 1] != LS_N - 1) {
        ini = 0; fin = pos_0;
        while (ini < fin) {
            tmp = tour[ini];
            tour[ini] = tour[fin];
            tour[fin] = tmp;
            ini++; fin--;
        }
        ini = pos_0 + 1; fin = n - 1;
        while (ini < fin) {
            tmp = tour[ini];
            tour[ini] = tour[fin];
            tour[fin] = tmp;
            ini++; fin--;
        }
    } else {
        long int *cp_tour = (long int *) calloc(n, sizeof(long int));
        for (i = 0; i < n; i++) cp_tour[i] = tour[i];
        for (k = 0, i = pos_0; i < n; i++, k++) tour[k] = cp_tour[i];
        for (i = 0; i < pos_0; i++, k++) tour[k] = cp_tour[i];
        free(cp_tour);
    }
}

/* --- on-the-fly local distances (no O(n^2) matrix) ---
 * The original realbench matrix[i][j] = distance(tour[i], tour[j]) is built
 * once from the tour BEFORE it is rewritten as position indices; the matrix
 * stays valid because positions keep their identity across moves. The same
 * value is recomputed here on the fly from the saved original tour, so the
 * LS behavior is unchanged while peak memory drops by ~4.5 GB at n=33810.
 */
static long int *ls_city_map = NULL;

static inline long int ls_dist(long int a, long int b) {
    return (long int)distance((int)ls_city_map[a], (int)ls_city_map[b]);
}

/* --- ls_inject_sorted: ESACO S1 -- insert v in sorted-by-ls_dist order, drop farthest, no repetition --- */
static void ls_inject_sorted(long int node, long int *list, long int size, long int v) {
    long int k;
    for (k = 0; k < size; ++k)
        if (list[k] == v) return; /* already present, keep as-is */
    long int d = ls_dist(node, v);
    long int i = 0;
    while (i < size && ls_dist(node, list[i]) < d) ++i; /* sorted position */
    if (i == size) return; /* farther than every NN, do not insert */
    for (k = size - 1; k > i; --k)
        list[k] = list[k - 1]; /* shift right, drop list[size-1] */
    list[i] = v;
}


/* --- city-space NN cache (idea.md A1) ---
 * ls_dist(a,b) = distance(city(a), city(b)) depends only on city geometry,
 * not on tour order. Precompute the k = min(max(nn_ls, nn_ants), nCities-1)
 * nearest real cities per city ONCE per run; each LS call then only maps
 * city ids to tour positions via reverse_pos: O(n*nn) instead of the old
 * O(n^2 log n) sort per node. */
static long int ls_nn_city[MAX_CITIES + 1][MAX_KN + 1];
static long int ls_city_k = 0;
static int ls_city_nn_ready = 0;

static void build_city_nn(void) {
    long int k = LS_MAX(nn_ls, nn_ants);
    long int m = (long int)nCities;
    if (k >= m) k = m - 1;
    if (k > MAX_KN) k = MAX_KN;
    ls_city_k = k;
    ls_city_nn_ready = 1;
    if (k <= 0) return;
    static long int dv[MAX_CITIES + 1];
    static long int hv[MAX_CITIES + 1];
    long int city, i, j;
    for (city = 0; city < m; ++city) {
        for (j = 0; j < m; ++j) {
            dv[j] = (long int)distance((int)city, (int)j);
            hv[j] = j;
        }
        dv[city] = LONG_MAX;
        ls_sort2(dv, hv, 0, m - 1);
        for (i = 0; i < k; ++i) ls_nn_city[city][i] = hv[i];
    }
}

/* --- compute_local_nn_lists (realbench ls.c) --- */
static long int** compute_local_nn_lists(long int *tour, long int n) {
    long int i, node, nn;
    long int *reverse_pos;
    long int **m_nnear;

    nn = LS_MAX(nn_ls, nn_ants);
    if (nn >= n) nn = n - 1;
    LS_DEBUG(assert(n > nn);)

    if ((m_nnear = (long int **) malloc(sizeof(long int) * n * nn + n * sizeof(long int *))) == NULL) {
        exit(EXIT_FAILURE);
    }
    reverse_pos = (long int *) calloc(LS_N, sizeof(long int));
    for (i = 0; i < LS_N; ++i) reverse_pos[i] = -1;
    for (i = 0; i < n; ++i) reverse_pos[tour[i]] = i;

    /* ESACO S1: successor of each city in the best-so-far tour, built once
       per LS call (O(n)) instead of scanning the best tour per node (O(n^2)). */
    static long int best_succ[MAX_CITIES + 1];
    fill_best_tour_succ(best_succ, LS_N);

    if (!ls_city_nn_ready) build_city_nn();
    const long int dummy_pos = reverse_pos[nCities];

    for (node = 0; node < n; node++) {
        long int cnt = 0;
        long int city = tour[node];
        m_nnear[node] = (long int *)(m_nnear + n) + node * nn;
        if (city >= 0 && city < (long int)nCities) {
            for (i = 0; i < ls_city_k && cnt < nn; ++i) {
                long int c2 = ls_nn_city[city][i];
                if (c2 == city || c2 >= (long int)nCities) continue;
                if (reverse_pos[c2] < 0) continue;
                m_nnear[node][cnt++] = reverse_pos[c2];
            }
        }
        /* pad: the original lists always had `nn` entries; the dummy node
           (distance 1e9) sat at the far end and just broke the ascending-
           distance loop in two_opt_first, so pad with dummy_pos. */
        for (; cnt < nn; ++cnt) m_nnear[node][cnt] = (dummy_pos >= 0) ? dummy_pos : node;

        /* ESACO S1: bias the LS candidate-set with pheromone + best-tour info */
        if (params.esaco) {
            if (city >= 0 && city < LS_N && city != (long int)nCities) {
                long int best = (long int)Pheromone.argmax_pheromone((int)city);
                if (best >= 0 && best < (long int)nCities && reverse_pos[best] != -1)
                    ls_inject_sorted(node, m_nnear[node], nn, reverse_pos[best]);
                long int succ = best_succ[city];
                if (succ >= 0 && succ < (long int)nCities && reverse_pos[succ] != -1)
                    ls_inject_sorted(node, m_nnear[node], nn, reverse_pos[succ]);
            }
        }
    }
    free(reverse_pos);
    return m_nnear;
}

/* --- generate_random_permutation (realbench ls.c) --- */
static long int * generate_random_permutation(long int n) {
    long int i, help, node, tot_assigned = 0;
    double rnd;
    long int *r;

    r = (long int *) malloc(n * sizeof(long int));
    for (i = 0; i < n; i++) r[i] = i;
    for (i = 0; i < n; i++) {
        rnd = ran01(&seed);
        node = (long int)(rnd * (n - tot_assigned));
        assert(i + node < n);
        help = r[i];
        r[i] = r[i + node];
        r[i + node] = help;
        tot_assigned++;
    }
    return r;
}

/* ==================================================================== */

void two_opt_first(long int *tour, long int t_size) {
    long int n = (long int)(t_size - 1);
    fprintf(stderr, "[dbg] two_opt_first n=%ld\n", n);
    long int i, j, h, l, nn, local_nn_ls;
    long int *original_tour = (long int *) malloc(n * sizeof(long int));
    for (i = 0; i < n; i++) { original_tour[i] = tour[i]; }
    ls_city_map = original_tour;
    long int **nn_list = compute_local_nn_lists(tour, n);
    for (i = 0; i < n; i++) tour[i] = i;
    local_nn_ls = LS_MIN(n - 1, nn_ls);

    long int c1, c2, s_c1, s_c2, p_c1, p_c2, pos_c1, pos_c2;
    long int improvement_flag, help, n_improves = 0, n_exchanges = 0;
    long int h1 = 0, h2 = 0, h3 = 0, h4 = 0;
    long int radius, gain = 0;
    long int *random_vector, *pos, *dlb;

    pos = (long int *) malloc(n * sizeof(long int));
    dlb = (long int *) malloc(n * sizeof(long int));
    for (i = 0; i < n; i++) { pos[tour[i]] = i; dlb[i] = LS_FALSE; }

    improvement_flag = LS_TRUE;
    random_vector = generate_random_permutation(n);

    while (improvement_flag) {
        improvement_flag = LS_FALSE;
        for (l = 0; l < n; l++) {            c1 = random_vector[l];
            LS_DEBUG(assert(c1 < n && c1 >= 0);)
            if (dlb_flag && dlb[c1]) continue;
            pos_c1 = pos[c1];
            s_c1 = tour[pos_c1 + 1];
            radius = ls_dist(c1, s_c1);

            for (h = 0; h < local_nn_ls; h++) {
                c2 = nn_list[c1][h];
                if (radius > ls_dist(c1, c2)) {
                    s_c2 = tour[pos[c2] + 1];
                    gain = -radius + ls_dist(c1, c2) + ls_dist(s_c1, s_c2) - ls_dist(c2, s_c2);
                    if (gain < 0) { h1 = c1; h2 = s_c1; h3 = c2; h4 = s_c2; goto exchange2opt; }
                } else break;
            }

            if (pos_c1 > 0) p_c1 = tour[pos_c1 - 1]; else p_c1 = tour[n - 1];
            radius = ls_dist(p_c1, c1);
            for (h = 0; h < local_nn_ls; h++) {
                c2 = nn_list[c1][h];
                if (radius > ls_dist(c1, c2)) {
                    pos_c2 = pos[c2];
                    if (pos_c2 > 0) p_c2 = tour[pos_c2 - 1]; else p_c2 = tour[n - 1];
                    if (p_c2 == c1) continue;
                    if (p_c1 == c2) continue;
                    gain = -radius + ls_dist(c1, c2) + ls_dist(p_c1, p_c2) - ls_dist(p_c2, c2);
                    if (gain < 0) { h1 = p_c1; h2 = c1; h3 = p_c2; h4 = c2; goto exchange2opt; }
                } else break;
            }

            dlb[c1] = LS_TRUE;
            continue;

        exchange2opt:
            n_exchanges++;
            improvement_flag = LS_TRUE;
            dlb[h1] = LS_FALSE; dlb[h2] = LS_FALSE; dlb[h3] = LS_FALSE; dlb[h4] = LS_FALSE;
            if (pos[h3] < pos[h1]) { help = h1; h1 = h3; h3 = help; help = h2; h2 = h4; h4 = help; }
            if (pos[h3] - pos[h2] < n / 2 + 1) {
                i = pos[h2]; j = pos[h3];
                while (i < j) {
                    c1 = tour[i]; c2 = tour[j];
                    tour[i] = c2; tour[j] = c1;
                    pos[c1] = j; pos[c2] = i;
                    i++; j--;
                }
                tour[n] = tour[0];
            } else {
                i = pos[h1]; j = pos[h4];
                if (j > i) help = n - (j - i) + 1; else help = (i - j) + 1;
                help = help / 2;
                for (h = 0; h < help; h++) {
                    c1 = tour[i]; c2 = tour[j];
                    tour[i] = c2; tour[j] = c1;
                    pos[c1] = j; pos[c2] = i;
                    i--; j++;
                    if (i < 0) i = n - 1;
                    if (j >= n) j = 0;
                }
                tour[n] = tour[0];
            }
        }
        if (improvement_flag) n_improves++;
    }

    for (i = 0; i < n; i++) tour[i] = original_tour[tour[i]];
    if (tour[0] != 0 || tour[1] == LS_N - 1) rotate_tour(tour, n);
    tour[n] = tour[0];
    fprintf(stderr, "[dbg] two_opt_first n=%ld done improvs=%ld\n", n, n_improves);

    free(random_vector); free(dlb); free(pos);
    free(nn_list); free(original_tour);
}

/* ==================================================================== */

void two_h_opt_first(long int *tour, long int t_size) {
    long int n = (long int)(t_size - 1);
    long int i, j, h, l, nn, local_nn_ls;
    long int *original_tour = (long int *) malloc(n * sizeof(long int));
    for (i = 0; i < n; i++) { original_tour[i] = tour[i]; }
    ls_city_map = original_tour;
    long int **nn_list = compute_local_nn_lists(tour, n);
    for (i = 0; i < n; i++) tour[i] = i;
    local_nn_ls = LS_MIN(n - 1, nn_ls);

    long int c1, c2, s_c1, s_c2, p_c1, p_c2, pos_c1, pos_c2;
    long int improvement_flag, improve_node;
    long int h1 = 0, h2 = 0, h3 = 0, h4 = 0, h5 = 0, help;
    long int radius, gain = 0;
    long int *random_vector;
    long int two_move, node_move;
    long int *pos, *dlb;

    pos = (long int *) malloc(n * sizeof(long int));
    dlb = (long int *) malloc(n * sizeof(long int));
    for (i = 0; i < n; i++) { pos[tour[i]] = i; dlb[i] = LS_FALSE; }

    improvement_flag = LS_TRUE;
    random_vector = generate_random_permutation(n);

    while (improvement_flag) {
        improvement_flag = LS_FALSE; two_move = LS_FALSE; node_move = LS_FALSE;
        for (l = 0; l < n; l++) {
            c1 = random_vector[l];
            LS_DEBUG(assert(c1 < n && c1 >= 0);)
            if (dlb_flag && dlb[c1]) continue;
            improve_node = LS_FALSE;
            pos_c1 = pos[c1];
            s_c1 = tour[pos_c1 + 1];
            radius = ls_dist(c1, s_c1);

            for (h = 0; h < local_nn_ls; h++) {
                c2 = nn_list[c1][h];
                if (radius > ls_dist(c1, c2)) {
                    pos_c2 = pos[c2];
                    s_c2 = tour[pos_c2 + 1];
                    gain = -radius + ls_dist(c1, c2) + ls_dist(s_c1, s_c2) - ls_dist(c2, s_c2);
                    if (gain < 0) {
                        h1 = c1; h2 = s_c1; h3 = c2; h4 = s_c2;
                        improve_node = LS_TRUE; two_move = LS_TRUE; node_move = LS_FALSE;
                        goto exchange;
                    }
                    if (pos_c2 > 0) p_c2 = tour[pos_c2 - 1]; else p_c2 = tour[n - 1];
                    gain = -radius + ls_dist(c1, c2) + ls_dist(c2, s_c1)
                         + ls_dist(p_c2, s_c2) - ls_dist(c2, s_c2) - ls_dist(p_c2, c2);
                    if (c2 == s_c1) gain = 0;
                    if (p_c2 == s_c1) gain = 0;
                    if (gain < 0) {
                        h1 = c1; h2 = s_c1; h3 = c2; h4 = p_c2; h5 = s_c2;
                        improve_node = LS_TRUE; node_move = LS_TRUE; two_move = LS_FALSE;
                        goto exchange;
                    }
                } else break;
            }

            if (pos_c1 > 0) p_c1 = tour[pos_c1 - 1]; else p_c1 = tour[n - 1];
            radius = ls_dist(p_c1, c1);
            for (h = 0; h < local_nn_ls; h++) {
                c2 = nn_list[c1][h];
                if (radius > ls_dist(c1, c2)) {
                    pos_c2 = pos[c2];
                    if (pos_c2 > 0) p_c2 = tour[pos_c2 - 1]; else p_c2 = tour[n - 1];
                    if (p_c2 == c1) continue;
                    if (p_c1 == c2) continue;
                    gain = -radius + ls_dist(c1, c2) + ls_dist(p_c1, p_c2) - ls_dist(p_c2, c2);
                    if (gain < 0) {
                        h1 = p_c1; h2 = c1; h3 = p_c2; h4 = c2;
                        improve_node = LS_TRUE; two_move = LS_TRUE; node_move = LS_FALSE;
                        goto exchange;
                    }
                    s_c2 = tour[pos[c2] + 1];
                    gain = -radius + ls_dist(c2, c1) + ls_dist(p_c1, c2)
                         + ls_dist(p_c2, s_c2) - ls_dist(c2, s_c2) - ls_dist(p_c2, c2);
                    if (p_c1 == c2) gain = 0;
                    if (p_c1 == s_c2) gain = 0;
                    if (gain < 0) {
                        h1 = p_c1; h2 = c1; h3 = c2; h4 = p_c2; h5 = s_c2;
                        improve_node = LS_TRUE; node_move = LS_TRUE; two_move = LS_FALSE;
                        goto exchange;
                    }
                } else break;
            }

        exchange:
            if (improve_node) {
                if (two_move) {
                    improvement_flag = LS_TRUE;
                    dlb[h1] = LS_FALSE; dlb[h2] = LS_FALSE; dlb[h3] = LS_FALSE; dlb[h4] = LS_FALSE;
                    if (pos[h3] < pos[h1]) { help = h1; h1 = h3; h3 = help; help = h2; h2 = h4; h4 = help; }
                    if (pos[h3] - pos[h2] < n / 2 + 1) {
                        i = pos[h2]; j = pos[h3];
                        while (i < j) {
                            c1 = tour[i]; c2 = tour[j];
                            tour[i] = c2; tour[j] = c1;
                            pos[c1] = j; pos[c2] = i;
                            i++; j--;
                        }
                        tour[n] = tour[0];
                    } else {
                        i = pos[h1]; j = pos[h4];
                        if (j > i) help = n - (j - i) + 1; else help = (i - j) + 1;
                        help = help / 2;
                        for (h = 0; h < help; h++) {
                            c1 = tour[i]; c2 = tour[j];
                            tour[i] = c2; tour[j] = c1;
                            pos[c1] = j; pos[c2] = i;
                            i--; j++;
                            if (i < 0) i = n - 1;
                            if (j >= n) j = 0;
                        }
                        tour[n] = tour[0];
                    }
                } else if (node_move) {
                    improvement_flag = LS_TRUE;
                    dlb[h1] = LS_FALSE; dlb[h2] = LS_FALSE; dlb[h3] = LS_FALSE; dlb[h4] = LS_FALSE; dlb[h5] = LS_FALSE;
                    if (pos[h3] < pos[h1]) {
                        help = pos[h1] - pos[h3];
                        i = pos[h3];
                        for (h = 0; h < help; h++) {
                            c1 = tour[i + 1];
                            tour[i] = c1;
                            pos[c1] = i;
                            i++;
                        }
                        tour[i] = h3;
                        pos[h3] = i;
                        tour[n] = tour[0];
                    } else {
                        help = pos[h3] - pos[h1];
                        i = pos[h3];
                        for (h = 0; h < help - 1; h++) {
                            c1 = tour[i - 1];
                            tour[i] = c1;
                            pos[c1] = i;
                            i--;
                        }
                        tour[i] = h3;
                        pos[h3] = i;
                        tour[n] = tour[0];
                    }
                } else {
                    fprintf(stderr, "this should never occur, 2-h-opt!!\n");
                    exit(0);
                }
                two_move = LS_FALSE; node_move = LS_FALSE;
            } else {
                dlb[c1] = LS_TRUE;
            }
        }
    }

    for (i = 0; i < n; i++) tour[i] = original_tour[tour[i]];
    if (tour[0] != 0 || tour[1] == LS_N - 1) rotate_tour(tour, n);
    tour[n] = tour[0];

    free(random_vector); free(dlb); free(pos);
    free(nn_list); free(original_tour);
}

/* ==================================================================== */

void three_opt_first(long int *tour, long int t_size) {
    long int n = (long int)(t_size - 1);
    long int i, j, g, h, l, nn, local_nn_ls;
    long int *original_tour = (long int *) malloc(n * sizeof(long int));
    for (i = 0; i < n; i++) { original_tour[i] = tour[i]; }
    ls_city_map = original_tour;
    long int **nn_list = compute_local_nn_lists(tour, n);
    for (i = 0; i < n; i++) tour[i] = i;
    local_nn_ls = LS_MIN(n - 1, nn_ls);

    long int c1, c2, c3, s_c1, s_c2, s_c3, p_c1, p_c2, p_c3;
    long int pos_c1, pos_c2, pos_c3;
    long int improvement_flag, help;
    long int h1 = 0, h2 = 0, h3 = 0, h4 = 0, h5 = 0, h6 = 0;
    long int diffs, diffp;
    long int between = LS_FALSE;
    long int opt2_flag;
    long int move_flag;
    long int gain, move_value, radius, add1, add2;
    long int decrease_breaks;
    long int val[3];
    long int n1, n2, n3;
    long int *pos, *dlb, *h_tour, *hh_tour, *random_vector;

    pos = (long int *) malloc(n * sizeof(long int));
    dlb = (long int *) malloc(n * sizeof(long int));
    h_tour = (long int *) malloc(n * sizeof(long int));
    hh_tour = (long int *) malloc(n * sizeof(long int));

    for (i = 0; i < n; i++) { pos[tour[i]] = i; dlb[i] = LS_FALSE; }
    improvement_flag = LS_TRUE;
    random_vector = generate_random_permutation(n);

    while (improvement_flag) {
        move_value = 0;
        improvement_flag = LS_FALSE;

        for (l = 0; l < n; l++) {
            c1 = random_vector[l];
            if (dlb_flag && dlb[c1]) continue;
            opt2_flag = LS_FALSE;
            move_flag = 0;
            pos_c1 = pos[c1];
            s_c1 = tour[pos_c1 + 1];
            if (pos_c1 > 0) p_c1 = tour[pos_c1 - 1]; else p_c1 = tour[n - 1];

            h = 0;
            while (h < local_nn_ls) {
                c2 = nn_list[c1][h];
                pos_c2 = pos[c2];
                s_c2 = tour[pos_c2 + 1];
                if (pos_c2 > 0) p_c2 = tour[pos_c2 - 1]; else p_c2 = tour[n - 1];

                diffs = 0; diffp = 0;
                radius = ls_dist(c1, s_c1);
                add1 = ls_dist(c1, c2);

                if (radius > add1) {
                    decrease_breaks = -radius - ls_dist(c2, s_c2);
                    diffs = decrease_breaks + add1 + ls_dist(s_c1, s_c2);
                    diffp = -radius - ls_dist(c2, p_c2) + ls_dist(c1, p_c2) + ls_dist(s_c1, c2);
                } else break;

                if (p_c2 == c1) diffp = 0;
                if ((diffs < move_value) || (diffp < move_value)) {
                    improvement_flag = LS_TRUE;
                    if (diffs <= diffp) {
                        h1 = c1; h2 = s_c1; h3 = c2; h4 = s_c2;
                        move_value = diffs;
                        opt2_flag = LS_TRUE; move_flag = 0;
                    } else {
                        h1 = c1; h2 = s_c1; h3 = p_c2; h4 = c2;
                        move_value = diffp;
                        opt2_flag = LS_TRUE; move_flag = 0;
                    }
                }

                g = 0;
                while (g < local_nn_ls) {
                    c3 = nn_list[s_c1][g];
                    pos_c3 = pos[c3];
                    s_c3 = tour[pos_c3 + 1];
                    if (pos_c3 > 0) p_c3 = tour[pos_c3 - 1]; else p_c3 = tour[n - 1];

                    if (c3 == c1) { g++; continue; }
                    else {
                        add2 = ls_dist(s_c1, c3);
                        if (decrease_breaks + add1 < add2) {
                            if (pos_c2 > pos_c1) {
                                if (pos_c3 <= pos_c2 && pos_c3 > pos_c1) between = LS_TRUE;
                                else between = LS_FALSE;
                            } else if (pos_c2 < pos_c1) {
                                if (pos_c3 > pos_c1 || pos_c3 < pos_c2) between = LS_TRUE;
                                else between = LS_FALSE;
                            } else {
                                printf(" Strange !!, pos_1 %ld == pos_2 %ld, \n", pos_c1, pos_c2);
                            }

                            if (between) {
                                gain = decrease_breaks - ls_dist(c3, p_c3) + add1 + add2 + ls_dist(p_c3, s_c2);
                                if (gain < move_value) {
                                    improvement_flag = LS_TRUE;
                                    move_value = gain;
                                    opt2_flag = LS_FALSE;
                                    move_flag = 1;
                                    h1 = c1; h2 = s_c1; h3 = c2; h4 = s_c2; h5 = p_c3; h6 = c3;
                                    goto exchange;
                                }
                            } else {
                                gain = decrease_breaks - ls_dist(c3, s_c3) + add1 + add2 + ls_dist(s_c2, s_c3);
                                if (pos_c2 == pos_c3) gain = 20000;
                                if (gain < move_value) {
                                    improvement_flag = LS_TRUE;
                                    move_value = gain;
                                    opt2_flag = LS_FALSE;
                                    move_flag = 2;
                                    h1 = c1; h2 = s_c1; h3 = c2; h4 = s_c2; h5 = c3; h6 = s_c3;
                                    goto exchange;
                                }

                                gain = -radius - ls_dist(p_c2, c2) - ls_dist(p_c3, c3) + add1 + add2 + ls_dist(p_c2, p_c3);
                                if (c3 == c2 || c2 == c1 || c1 == c3 || p_c2 == c1) gain = 2000000;
                                if (gain < move_value) {
                                    improvement_flag = LS_TRUE;
                                    move_value = gain;
                                    opt2_flag = LS_FALSE;
                                    move_flag = 3;
                                    h1 = c1; h2 = s_c1; h3 = p_c2; h4 = c2; h5 = p_c3; h6 = c3;
                                    goto exchange;
                                }

                                gain = -radius - ls_dist(p_c2, c2) - ls_dist(c3, s_c3) + add1 + add2 + ls_dist(p_c2, s_c3);
                                if (gain < move_value) {
                                    improvement_flag = LS_TRUE;
                                    move_value = gain;
                                    opt2_flag = LS_FALSE;
                                    move_flag = 4;
                                    improvement_flag = LS_TRUE;
                                    h1 = c1; h2 = s_c1; h3 = p_c2; h4 = c2; h5 = c3; h6 = s_c3;
                                    goto exchange;
                                }
                            }
                        } else g = local_nn_ls + 1;
                    }
                    g++;
                }
                h++;
            }
            if (move_flag || opt2_flag) {
            exchange:
                move_value = 0;
                if (move_flag) {
                    dlb[h1] = LS_FALSE; dlb[h2] = LS_FALSE; dlb[h3] = LS_FALSE;
                    dlb[h4] = LS_FALSE; dlb[h5] = LS_FALSE; dlb[h6] = LS_FALSE;
                    pos_c1 = pos[h1]; pos_c2 = pos[h3]; pos_c3 = pos[h5];

                    if (move_flag == 4) {
                        if (pos_c2 > pos_c1) n1 = pos_c2 - pos_c1; else n1 = n - (pos_c1 - pos_c2);
                        if (pos_c3 > pos_c2) n2 = pos_c3 - pos_c2; else n2 = n - (pos_c2 - pos_c3);
                        if (pos_c1 > pos_c3) n3 = pos_c1 - pos_c3; else n3 = n - (pos_c3 - pos_c1);
                        val[0] = n1; val[1] = n2; val[2] = n3;
                        h = 0; help = LONG_MIN;
                        for (g = 0; g <= 2; g++) { if (help < val[g]) { help = val[g]; h = g; } }

                        if (h == 0) {
                            j = pos[h4]; h = pos[h5]; i = 0;
                            h_tour[i] = tour[j]; n1 = 1;
                            while (j != h) { i++; j++; if (j >= n) j = 0; h_tour[i] = tour[j]; n1++; }
                            j = pos[h4]; i = pos[h6];
                            tour[j] = tour[i]; pos[tour[i]] = j;
                            while (i != pos_c1) { i++; if (i >= n) i = 0; j++; if (j >= n) j = 0; tour[j] = tour[i]; pos[tour[i]] = j; }
                            j++; if (j >= n) j = 0;
                            for (i = 0; i < n1; i++) { tour[j] = h_tour[i]; pos[h_tour[i]] = j; j++; if (j >= n) j = 0; }
                            tour[n] = tour[0];
                        } else if (h == 1) {
                            j = pos[h6]; h = pos[h1]; i = 0;
                            h_tour[i] = tour[j]; n1 = 1;
                            while (j != h) { i++; j++; if (j >= n) j = 0; h_tour[i] = tour[j]; n1++; }
                            j = pos[h6]; i = pos[h2];
                            tour[j] = tour[i]; pos[tour[i]] = j;
                            while (i != pos_c2) { i++; if (i >= n) i = 0; j++; if (j >= n) j = 0; tour[j] = tour[i]; pos[tour[i]] = j; }
                            j++; if (j >= n) j = 0;
                            for (i = 0; i < n1; i++) { tour[j] = h_tour[i]; pos[h_tour[i]] = j; j++; if (j >= n) j = 0; }
                            tour[n] = tour[0];
                        } else if (h == 2) {
                            j = pos[h2]; h = pos[h3]; i = 0;
                            h_tour[i] = tour[j]; n1 = 1;
                            while (j != h) { i++; j++; if (j >= n) j = 0; h_tour[i] = tour[j]; n1++; }
                            j = pos[h2]; i = pos[h4];
                            tour[j] = tour[i]; pos[tour[i]] = j;
                            while (i != pos_c3) { i++; if (i >= n) i = 0; j++; if (j >= n) j = 0; tour[j] = tour[i]; pos[tour[i]] = j; }
                            j++; if (j >= n) j = 0;
                            for (i = 0; i < n1; i++) { tour[j] = h_tour[i]; pos[h_tour[i]] = j; j++; if (j >= n) j = 0; }
                            tour[n] = tour[0];
                        }
                    } else if (move_flag == 1) {
                        if (pos_c3 < pos_c2) n1 = pos_c2 - pos_c3; else n1 = n - (pos_c3 - pos_c2);
                        if (pos_c3 > pos_c1) n2 = pos_c3 - pos_c1 + 1; else n2 = n - (pos_c1 - pos_c3 + 1);
                        if (pos_c2 > pos_c1) n3 = n - (pos_c2 - pos_c1 + 1); else n3 = pos_c1 - pos_c2 + 1;
                        val[0] = n1; val[1] = n2; val[2] = n3;
                        h = 0; help = LONG_MIN;
                        for (g = 0; g <= 2; g++) { if (help < val[g]) { help = val[g]; h = g; } }

                        if (h == 0) {
                            j = pos[h5]; h = pos[h2]; i = 0;
                            h_tour[i] = tour[j]; n1 = 1;
                            while (j != h) { i++; j--; if (j < 0) j = n - 1; h_tour[i] = tour[j]; n1++; }
                            j = pos[h1]; h = pos[h4]; i = 0;
                            hh_tour[i] = tour[j]; n2 = 1;
                            while (j != h) { i++; j--; if (j < 0) j = n - 1; hh_tour[i] = tour[j]; n2++; }
                            j = pos[h4];
                            for (i = 0; i < n2; i++) { tour[j] = hh_tour[i]; pos[hh_tour[i]] = j; j++; if (j >= n) j = 0; }
                            for (i = 0; i < n1; i++) { tour[j] = h_tour[i]; pos[h_tour[i]] = j; j++; if (j >= n) j = 0; }
                            tour[n] = tour[0];
                        } else if (h == 1) {
                            j = pos[h3]; h = pos[h6]; i = 0;
                            h_tour[i] = tour[j]; n1 = 1;
                            while (j != h) { i++; j--; if (j < 0) j = n - 1; h_tour[i] = tour[j]; n1++; }
                            j = pos[h6]; i = pos[h4];
                            tour[j] = tour[i]; pos[tour[i]] = j;
                            while (i != pos_c1) { i++; j++; if (j >= n) j = 0; if (i >= n) i = 0; tour[j] = tour[i]; pos[tour[i]] = j; }
                            j++; if (j >= n) j = 0;
                            i = 0;
                            tour[j] = h_tour[i]; pos[h_tour[i]] = j;
                            while (j != pos_c1) { j++; if (j >= n) j = 0; i++; tour[j] = h_tour[i]; pos[h_tour[i]] = j; }
                            tour[n] = tour[0];
                        } else if (h == 2) {
                            j = pos[h2]; h = pos[h5]; i = 0;
                            h_tour[i] = tour[j]; n1 = 1;
                            while (j != h) { i++; j++; if (j >= n) j = 0; h_tour[i] = tour[j]; n1++; }
                            j = pos_c2; h = pos[h6]; i = 0;
                            hh_tour[i] = tour[j]; n2 = 1;
                            while (j != h) { i++; j--; if (j < 0) j = n - 1; hh_tour[i] = tour[j]; n2++; }
                            j = pos[h2];
                            for (i = 0; i < n2; i++) { tour[j] = hh_tour[i]; pos[hh_tour[i]] = j; j++; if (j >= n) j = 0; }
                            for (i = 0; i < n1; i++) { tour[j] = h_tour[i]; pos[h_tour[i]] = j; j++; if (j >= n) j = 0; }
                            tour[n] = tour[0];
                        }
                    } else if (move_flag == 2) {
                        if (pos_c3 < pos_c1) n1 = pos_c1 - pos_c3; else n1 = n - (pos_c3 - pos_c1);
                        if (pos_c3 > pos_c2) n2 = pos_c3 - pos_c2; else n2 = n - (pos_c2 - pos_c3);
                        if (pos_c2 > pos_c1) n3 = pos_c2 - pos_c1; else n3 = n - (pos_c1 - pos_c2);
                        val[0] = n1; val[1] = n2; val[2] = n3;
                        h = 0; help = LONG_MIN;
                        for (g = 0; g <= 2; g++) { if (help < val[g]) { help = val[g]; h = g; } }

                        if (h == 0) {
                            j = pos[h3]; h = pos[h2]; i = 0;
                            h_tour[i] = tour[j]; n1 = 1;
                            while (j != h) { i++; j--; if (j < 0) j = n - 1; h_tour[i] = tour[j]; n1++; }
                            j = pos[h5]; h = pos[h4]; i = 0;
                            hh_tour[i] = tour[j]; n2 = 1;
                            while (j != h) { i++; j--; if (j < 0) j = n - 1; hh_tour[i] = tour[j]; n2++; }
                            j = pos[h2];
                            for (i = 0; i < n1; i++) { tour[j] = h_tour[i]; pos[h_tour[i]] = j; j++; if (j >= n) j = 0; }
                            for (i = 0; i < n2; i++) { tour[j] = hh_tour[i]; pos[hh_tour[i]] = j; j++; if (j >= n) j = 0; }
                            tour[n] = tour[0];
                        } else if (h == 1) {
                            j = pos[h2]; h = pos[h3]; i = 0;
                            h_tour[i] = tour[j]; n1 = 1;
                            while (j != h) { i++; j++; if (j >= n) j = 0; h_tour[i] = tour[j]; n1++; }
                            j = pos[h1]; h = pos[h6]; i = 0;
                            hh_tour[i] = tour[j]; n2 = 1;
                            while (j != h) { i++; j--; if (j < 0) j = n - 1; hh_tour[i] = tour[j]; n2++; }
                            j = pos[h6];
                            for (i = 0; i < n1; i++) { tour[j] = h_tour[i]; pos[h_tour[i]] = j; j++; if (j >= n) j = 0; }
                            for (i = 0; i < n2; i++) { tour[j] = hh_tour[i]; pos[hh_tour[i]] = j; j++; if (j >= n) j = 0; }
                            tour[n] = tour[0];
                        } else if (h == 2) {
                            j = pos[h1]; h = pos[h6]; i = 0;
                            h_tour[i] = tour[j]; n1 = 1;
                            while (j != h) { i++; j--; if (j < 0) j = n - 1; h_tour[i] = tour[j]; n1++; }
                            j = pos[h4]; h = pos[h5]; i = 0;
                            hh_tour[i] = tour[j]; n2 = 1;
                            while (j != h) { i++; j++; if (j >= n) j = 0; hh_tour[i] = tour[j]; n2++; }
                            j = pos[h4];
                            for (i = 0; i < n1; i++) { tour[j] = h_tour[i]; pos[h_tour[i]] = j; j++; if (j >= n) j = 0; }
                            for (i = 0; i < n2; i++) { tour[j] = hh_tour[i]; pos[hh_tour[i]] = j; j++; if (j >= n) j = 0; }
                            tour[n] = tour[0];
                        }
                    } else if (move_flag == 3) {
                        if (pos_c3 < pos_c1) n1 = pos_c1 - pos_c3; else n1 = n - (pos_c3 - pos_c1);
                        if (pos_c3 > pos_c2) n2 = pos_c3 - pos_c2; else n2 = n - (pos_c2 - pos_c3);
                        if (pos_c2 > pos_c1) n3 = pos_c2 - pos_c1; else n3 = n - (pos_c1 - pos_c2);
                        val[0] = n1; val[1] = n2; val[2] = n3;
                        h = 0; help = LONG_MIN;
                        for (g = 0; g <= 2; g++) { if (help < val[g]) { help = val[g]; h = g; } }

                        if (h == 0) {
                            j = pos[h3]; h = pos[h2]; i = 0;
                            h_tour[i] = tour[j]; n1 = 1;
                            while (j != h) { i++; j--; if (j < 0) j = n - 1; h_tour[i] = tour[j]; n1++; }
                            j = pos[h2]; h = pos[h5]; i = pos[h4];
                            tour[j] = h4; pos[h4] = j;
                            while (i != h) { i++; if (i >= n) i = 0; j++; if (j >= n) j = 0; tour[j] = tour[i]; pos[tour[i]] = j; }
                            j++; if (j >= n) j = 0;
                            for (i = 0; i < n1; i++) { tour[j] = h_tour[i]; pos[h_tour[i]] = j; j++; if (j >= n) j = 0; }
                            tour[n] = tour[0];
                        } else if (h == 1) {
                            j = pos[h3]; h = pos[h2]; i = 0;
                            h_tour[i] = tour[j]; n1 = 1;
                            while (j != h) { i++; j--; if (j < 0) j = n - 1; h_tour[i] = tour[j]; n1++; }
                            j = pos[h6]; h = pos[h1]; i = 0;
                            hh_tour[i] = tour[j]; n2 = 1;
                            while (j != h) { i++; j++; if (j >= n) j = 0; hh_tour[i] = tour[j]; n2++; }
                            j = pos[h6];
                            for (i = 0; i < n1; i++) { tour[j] = h_tour[i]; pos[h_tour[i]] = j; j++; if (j >= n) j = 0; }
                            for (i = 0; i < n2; i++) { tour[j] = hh_tour[i]; pos[hh_tour[i]] = j; j++; if (j >= n) j = 0; }
                            tour[n] = tour[0];
                        } else if (h == 2) {
                            j = pos[h5]; h = pos[h4]; i = 0;
                            h_tour[i] = tour[j]; n1 = 1;
                            while (j != h) { i++; j--; if (j < 0) j = n - 1; h_tour[i] = tour[j]; n1++; }
                            j = pos[h1]; h = pos[h6]; i = 0;
                            hh_tour[i] = tour[j]; n2 = 1;
                            while (j != h) { i++; j--; if (j < 0) j = n - 1; hh_tour[i] = tour[j]; n2++; }
                            j = pos[h4];
                            for (i = 0; i < n1; i++) { tour[j] = h_tour[i]; pos[h_tour[i]] = j; j++; if (j >= n) j = 0; }
                            for (i = 0; i < n2; i++) { tour[j] = hh_tour[i]; pos[hh_tour[i]] = j; j++; if (j >= n) j = 0; }
                            tour[n] = tour[0];
                        }
                    } else {
                        printf(" Some very strange error must have occurred !!!\n\n");
                        exit(0);
                    }
                }
                if (opt2_flag) {
                    dlb[h1] = LS_FALSE; dlb[h2] = LS_FALSE; dlb[h3] = LS_FALSE; dlb[h4] = LS_FALSE;
                    if (pos[h3] < pos[h1]) { help = h1; h1 = h3; h3 = help; help = h2; h2 = h4; h4 = help; }
                    if (pos[h3] - pos[h2] < n / 2 + 1) {
                        i = pos[h2]; j = pos[h3];
                        while (i < j) {
                            c1 = tour[i]; c2 = tour[j];
                            tour[i] = c2; tour[j] = c1;
                            pos[c1] = j; pos[c2] = i;
                            i++; j--;
                        }
                        tour[n] = tour[0];
                    } else {
                        i = pos[h1]; j = pos[h4];
                        if (j > i) help = n - (j - i) + 1; else help = (i - j) + 1;
                        help = help / 2;
                        for (h = 0; h < help; h++) {
                            c1 = tour[i]; c2 = tour[j];
                            tour[i] = c2; tour[j] = c1;
                            pos[c1] = j; pos[c2] = i;
                            i--; j++;
                            if (i < 0) i = n - 1;
                            if (j >= n) j = 0;
                        }
                        tour[n] = tour[0];
                    }
                }
            } else {
                dlb[c1] = LS_TRUE;
            }
        }
    }

    for (i = 0; i < n; i++) tour[i] = original_tour[tour[i]];
    if (tour[0] != 0 || tour[1] == LS_N - 1) rotate_tour(tour, n);
    tour[n] = tour[0];

    free(random_vector); free(h_tour); free(hh_tour); free(pos); free(dlb);
    free(nn_list); free(original_tour);
}
