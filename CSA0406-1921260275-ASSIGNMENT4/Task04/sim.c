#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAXC 199
#define SEEK 0.1      /* [A] ms per cylinder of head movement */
#define FIXED 3.0     /* [A] ms rotation + transfer per request */
#define NREQ 10000
#define SEEDS 5
enum { FCFS, SSTF, SCAN, CSCAN, LOOK, CLOOK, NALG };
static const char *NAME[NALG] = {"FCFS", "SSTF", "SCAN", "C-SCAN", "LOOK", "C-LOOK"};

typedef struct { double arr; int cyl; } Req;
typedef struct {
    double avg_move, avg_wait, max_wait, sd_wait, track_wait;
    int track_before;
    double zsum[3]; long zcnt[3];
} Stat;

static unsigned long long rs = 88172645463325252ULL;
static double urand(void) {
    rs ^= rs >> 12; rs ^= rs << 25; rs ^= rs >> 27;
    return (double)((rs * 2685821657736338717ULL) >> 11) * (1.0 / 9007199254740992.0);
}

/* nearest pending request lying ahead of the head in direction dir (ties: earlier arrival) */
static int nearest_ahead(const int *pend, int np, const Req *r, int pos, int dir) {
    int best = -1, bd = 0;
    for (int p = 0; p < np; p++) {
        int c = r[pend[p]].cyl;
        if ((c - pos) * dir < 0) continue;
        int d = abs(c - pos);
        if (best < 0 || d < bd || (d == bd && pend[p] < pend[best])) { best = p; bd = d; }
    }
    return best;
}

static void simulate(const Req *r, int n, int alg, int head0, int track, Stat *st) {
    int *pend = malloc(sizeof(int) * (size_t)n);
    int np = 0, next = 0, done = 0, pos = head0, dir = -1;
    double t = 0, total_move = 0, sw = 0, sw2 = 0, mx = 0;
    memset(st, 0, sizeof *st);
    st->track_wait = -1; st->track_before = -1;
    while (done < n) {
        while (next < n && r[next].arr <= t) pend[np++] = next++;
        if (np == 0) { t = r[next].arr; continue; }
        int extra = 0, best = -1;
        if (alg == FCFS) {
            for (int p = 0; p < np; p++) if (best < 0 || pend[p] < pend[best]) best = p;
        } else if (alg == SSTF) {
            int bd = 0;
            for (int p = 0; p < np; p++) {
                int d = abs(r[pend[p]].cyl - pos);
                if (best < 0 || d < bd || (d == bd && pend[p] < pend[best])) { best = p; bd = d; }
            }
        } else {
            if (alg == CSCAN || alg == CLOOK) dir = 1;
            best = nearest_ahead(pend, np, r, pos, dir);
            if (best < 0) {
                if (alg == SCAN) { int edge = dir > 0 ? MAXC : 0; extra = abs(edge - pos); pos = edge; dir = -dir; }
                else if (alg == LOOK) { dir = -dir; }
                else if (alg == CSCAN) { extra = (MAXC - pos) + MAXC; pos = 0; }
                else {
                    int lo = MAXC + 1;
                    for (int p = 0; p < np; p++) if (r[pend[p]].cyl < lo) lo = r[pend[p]].cyl;
                    extra = abs(pos - lo); pos = lo;
                }
                best = nearest_ahead(pend, np, r, pos, dir);
            }
        }
        int ri = pend[best];
        int dist = abs(r[ri].cyl - pos);
        double start = t + SEEK * extra;
        double wait = start - r[ri].arr;
        total_move += extra + dist;
        t = start + SEEK * dist + FIXED;
        pos = r[ri].cyl;
        pend[best] = pend[--np];
        sw += wait; sw2 += wait * wait;
        if (wait > mx) mx = wait;
        int z = r[ri].cyl * 3 / (MAXC + 1);
        st->zsum[z] += wait; st->zcnt[z]++;
        if (ri == track) { st->track_wait = wait; st->track_before = done; }
        done++;
    }
    st->avg_move = total_move / n;
    st->avg_wait = sw / n;
    double var = sw2 / n - st->avg_wait * st->avg_wait;
    st->sd_wait = sqrt(var > 0 ? var : 0);
    st->max_wait = mx;
    free(pend);
}

static void gen_random(Req *r, int n, double mean_ia) {
    double t = 0;
    for (int i = 0; i < n; i++) {
        t += -mean_ia * log(1.0 - urand());
        r[i].arr = t;
        r[i].cyl = (int)(urand() * (MAXC + 1));
    }
}

static double median5(double a[SEEDS]) {
    for (int i = 1; i < SEEDS; i++) { double v = a[i]; int j = i - 1; while (j >= 0 && a[j] > v) { a[j + 1] = a[j]; j--; } a[j + 1] = v; }
    return a[SEEDS / 2];
}

static const char *LOADN[3] = {"light", "medium", "heavy"};
static const double LOADIA[3] = {40.0, 12.0, 5.0};   /* [A] mean inter-arrival time in ms */

static void seed_rng(int s) { rs = (unsigned long long)(1000 + 17 * s) * 0x9E3779B97F4A7C15ULL; }

static void mode_A(void) {
    Req *r = malloc(sizeof(Req) * NREQ);
    FILE *csv = fopen("sim_runs.csv", "w");
    if (!csv) { printf("cannot write sim_runs.csv\n"); return; }
    fprintf(csv, "load,seed,algorithm,avg_move,avg_wait_ms,max_wait_ms,sd_wait_ms\n");
    printf("%d random requests, cylinders 0-%d, start head 100. Median of %d seeds. [A] seek %.1f ms/cyl + %.1f ms fixed.\n",
           NREQ, MAXC, SEEDS, SEEK, FIXED);
    printf("Head movement includes the SCAN trip to the edge and the C-SCAN/C-LOOK return jump.\n");
    for (int l = 0; l < 3; l++) {
        double mv[NALG][SEEDS], aw[NALG][SEEDS], mw[NALG][SEEDS], sd[NALG][SEEDS];
        for (int s = 0; s < SEEDS; s++) {
            seed_rng(s);
            gen_random(r, NREQ, LOADIA[l]);
            for (int a = 0; a < NALG; a++) {
                Stat st;
                simulate(r, NREQ, a, 100, -1, &st);
                mv[a][s] = st.avg_move; aw[a][s] = st.avg_wait; mw[a][s] = st.max_wait; sd[a][s] = st.sd_wait;
                fprintf(csv, "%s,%d,%s,%.3f,%.3f,%.3f,%.3f\n", LOADN[l], s + 1, NAME[a], st.avg_move, st.avg_wait, st.max_wait, st.sd_wait);
            }
        }
        printf("\n%s load: mean inter-arrival %.0f ms\n", LOADN[l], LOADIA[l]);
        printf("%-7s %12s %14s %14s %14s %9s\n", "alg", "avg move", "avg wait ms", "max wait ms", "sd wait ms", "sd/avg");
        for (int a = 0; a < NALG; a++)
        {
            double m1 = median5(mv[a]), m2 = median5(aw[a]), m3 = median5(mw[a]), m4 = median5(sd[a]);
            printf("%-7s %12.2f %14.1f %14.1f %14.1f %9.2f\n", NAME[a], m1, m2, m3, m4, m4 / m2);
        }
    }
    fclose(csv);
    free(r);
    printf("\nRaw per-seed values are in sim_runs.csv\n");
}

static void mode_B(void) {
    int init[8] = {98, 183, 37, 122, 14, 124, 65, 67};
    int lens[3] = {100, 1000, 10000};
    printf("Head at 53, queue 98 183 37 122 14 124 65 67 at t=0, then one request every 1 ms near cylinder 60 (58..62 repeating).\n");
    printf("Tracked request: cylinder 183. [A] seek %.1f ms/cyl + %.1f ms fixed.\n", SEEK, FIXED);
    for (int k = 0; k < 3; k++) {
        int S = lens[k], n = 8 + S;
        Req *r = malloc(sizeof(Req) * (size_t)n);
        for (int i = 0; i < 8; i++) { r[i].arr = 0; r[i].cyl = init[i]; }
        for (int i = 0; i < S; i++) { r[8 + i].arr = i + 1; r[8 + i].cyl = 58 + (i % 5); }
        printf("\nStream of %d requests (lasts %d ms)\n", S, S);
        printf("%-7s %20s %24s\n", "alg", "wait of 183 (ms)", "requests served before it");
        for (int a = 0; a < NALG; a++) {
            Stat st;
            simulate(r, n, a, 53, 1, &st);
            printf("%-7s %20.1f %24d\n", NAME[a], st.track_wait, st.track_before);
        }
        free(r);
    }
}

static void mode_C(void) {
    Req *r = malloc(sizeof(Req) * NREQ);
    int algs[4] = {SCAN, CSCAN, LOOK, CLOOK};
    printf("Mean wait (ms) by cylinder zone, pooled over %d seeds x %d requests.\n", SEEDS, NREQ);
    printf("Zones: low = cylinders 0-66, middle = 67-132, high = 133-199.\n");
    for (int l = 0; l < 3; l++) {
        double zs[4][3] = {{0}}; long zc[4][3] = {{0}};
        for (int s = 0; s < SEEDS; s++) {
            seed_rng(s);
            gen_random(r, NREQ, LOADIA[l]);
            for (int a = 0; a < 4; a++) {
                Stat st;
                simulate(r, NREQ, algs[a], 100, -1, &st);
                for (int z = 0; z < 3; z++) { zs[a][z] += st.zsum[z]; zc[a][z] += st.zcnt[z]; }
            }
        }
        printf("\n%s load\n%-7s %10s %10s %10s %14s\n", LOADN[l], "alg", "low", "middle", "high", "middle/edges");
        for (int a = 0; a < 4; a++) {
            double lo = zs[a][0] / (double)zc[a][0], mi = zs[a][1] / (double)zc[a][1], hi = zs[a][2] / (double)zc[a][2];
            printf("%-7s %10.1f %10.1f %10.1f %14.2f\n", NAME[algs[a]], lo, mi, hi, mi / ((lo + hi) / 2));
        }
    }
    free(r);
}

/* walk through stops, print the sum of distances, return it */
static int walk(const int *pts, int m) {
    int total = 0;
    for (int i = 0; i + 1 < m; i++) {
        int d = abs(pts[i + 1] - pts[i]);
        printf("%s|%d-%d|=%d", i ? " + " : "", pts[i], pts[i + 1], d);
        total += d;
    }
    return total;
}

static void mode_T(void) {
    int fcfs[] = {53, 98, 183, 37, 122, 14, 124, 65, 67};
    int sstf[] = {53, 65, 67, 37, 14, 98, 122, 124, 183};
    int scan[] = {53, 37, 14, 0, 65, 67, 98, 122, 124, 183};
    int cs1[] = {53, 65, 67, 98, 122, 124, 183, 199};
    int cs2[] = {0, 14, 37};
    int look[] = {53, 37, 14, 65, 67, 98, 122, 124, 183};
    int cl1[] = {53, 65, 67, 98, 122, 124, 183};
    int cl2[] = {14, 37};
    int t;
    printf("Head 53, queue 98 183 37 122 14 124 65 67, cylinders 0-199.\n");
    printf("SCAN and LOOK go towards 0 first; C-SCAN and C-LOOK go towards 199 first (as in the given code).\n\n");
    printf("FCFS   order: 98 183 37 122 14 124 65 67\n  "); t = walk(fcfs, 9); printf(" = %d\n\n", t);
    printf("SSTF   order: 65 67 37 14 98 122 124 183\n  "); t = walk(sstf, 9); printf(" = %d\n\n", t);
    printf("SCAN   order: 37 14 (0) 65 67 98 122 124 183\n  "); t = walk(scan, 10); printf(" = %d\n\n", t);
    {
        int a, b;
        printf("C-SCAN order: 65 67 98 122 124 183 (199) jump (0) 14 37\n  ");
        a = walk(cs1, 8); printf(" = %d\n  jump 199 -> 0 = 199 (counted separately)\n  ", a);
        b = walk(cs2, 3); printf(" = %d\n  total without jump = %d, with jump = %d\n\n", b, a + b, a + b + 199);
    }
    printf("LOOK   order: 37 14 65 67 98 122 124 183\n  "); t = walk(look, 9); printf(" = %d\n\n", t);
    {
        int a, b, j = abs(183 - 14);
        printf("C-LOOK order: 65 67 98 122 124 183 jump 14 37\n  ");
        a = walk(cl1, 7); printf(" = %d\n  jump 183 -> 14 = %d (counted separately)\n  ", a, j);
        b = walk(cl2, 2); printf(" = %d\n  total without jump = %d, with jump = %d\n", b, a + b, a + b + j);
    }
}

int main(int argc, char **argv) {
    char m = argc > 1 ? argv[1][0] : '?';
    if (m == 'T') mode_T();
    else if (m == 'A') mode_A();
    else if (m == 'B') mode_B();
    else if (m == 'C') mode_C();
    else { printf("usage: ./sim T|A|B|C\n"); return 1; }
    return 0;
}
