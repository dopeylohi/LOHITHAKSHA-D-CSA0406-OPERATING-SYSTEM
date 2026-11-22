#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define V 2000               /* videos in the library [A] */
#define HOT 200              /* most popular videos held on the SSD tier in design B (10%) */
#define D 12                 /* hard disks per server [A] */
#define CHUNK_MB 4.194304    /* 4 MiB request size */
#define PERIOD 15.1          /* s between chunk requests of one stream (4 MiB at 0.278 MB/s) */
#define WINDOW 120.0         /* s simulated per run */
#define START_RATE 50.0      /* [A] session starts or jumps per second per server */
#define HDD_BW 150.0         /* [A] MB/s */
#define ROT 4.2              /* [A] ms average rotational latency (7200 rpm) */
#define META 8.2             /* [A] ms for one random metadata read on an HDD */
#define SSD_BW 3000.0        /* [A] MB/s NVMe */
#define SSD_LAT 0.1          /* [A] ms per read on the SSD */
#define DEADLINE 10000.0     /* [A] ms: a chunk later than this makes the player stall */
#define RUNS 5

typedef struct { double arr; int video; int start; } Req;
typedef struct { int *pend; int np; double free_at; int pos; int dir; double busy; } Dev;

static unsigned long long rs = 88172645463325252ULL;
static double urand(void) {
    rs ^= rs >> 12; rs ^= rs << 25; rs ^= rs >> 27;
    return (double)((rs * 2685821657736338717ULL) >> 11) * (1.0 / 9007199254740992.0);
}

static double cdf[V];
static int zipf_video(void) {
    double u = urand();
    int lo = 0, hi = V - 1;
    while (lo < hi) { int mid = (lo + hi) / 2; if (cdf[mid] < u) lo = mid + 1; else hi = mid; }
    return lo;
}

static int cmp_req(const void *a, const void *b) {
    double x = ((const Req *)a)->arr, y = ((const Req *)b)->arr;
    return (x > y) - (x < y);
}
static int cmp_dbl(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static int cyl[V], rep1[V], rep2[V];

typedef struct { double smed, sp95, sp99, late, hddbusy, ssdshare; } Res;

static void run(int streams, int design, Res *out) {
    int maxn = streams * ((int)(WINDOW / PERIOD) + 2) + (int)(START_RATE * WINDOW * 1.5) + 100;
    Req *r = malloc(sizeof(Req) * (size_t)maxn);
    int n = 0;
    for (int v = 0; v < V; v++) {
        cyl[v] = (int)(urand() * 200);
        rep1[v] = v % D;
        rep2[v] = (rep1[v] + 1 + (v / D) % (D - 1)) % D;
    }
    for (int s = 0; s < streams; s++) {
        int v = zipf_video();
        for (double t = urand() * PERIOD; t < WINDOW; t += PERIOD) {
            r[n].arr = t * 1000.0; r[n].video = v; r[n].start = 0; n++;
        }
    }
    for (double t = -log(1.0 - urand()) / START_RATE; t < WINDOW; t += -log(1.0 - urand()) / START_RATE) {
        r[n].arr = t * 1000.0; r[n].video = zipf_video(); r[n].start = 1; n++;
    }
    qsort(r, (size_t)n, sizeof(Req), cmp_req);

    Dev dev[D + 1];
    for (int d = 0; d <= D; d++) { dev[d].pend = malloc(sizeof(int) * (size_t)n); dev[d].np = 0; dev[d].free_at = 0; dev[d].pos = 100; dev[d].dir = -1; dev[d].busy = 0; }
    double *sl = malloc(sizeof(double) * (size_t)n);
    int ns = 0, chunks = 0, late = 0, onssd = 0, i = 0, served = 0;

    while (served < n) {
        double next_arr = i < n ? r[i].arr : 1e300;
        int bd = -1;
        for (int d = 0; d <= D; d++) if (dev[d].np > 0 && (bd < 0 || dev[d].free_at < dev[bd].free_at)) bd = d;
        if (bd >= 0 && dev[bd].free_at <= next_arr) {
            Dev *dv = &dev[bd];
            int best = -1;
            if (bd == D) {
                for (int p = 0; p < dv->np; p++) if (best < 0 || dv->pend[p] < dv->pend[best]) best = p;
            } else {
                for (int pass = 0; pass < 2 && best < 0; pass++) {
                    int bdist = 0;
                    for (int p = 0; p < dv->np; p++) {
                        int c = cyl[r[dv->pend[p]].video];
                        if ((c - dv->pos) * dv->dir < 0) continue;
                        int dist = abs(c - dv->pos);
                        if (best < 0 || dist < bdist || (dist == bdist && dv->pend[p] < dv->pend[best])) { best = p; bdist = dist; }
                    }
                    if (best < 0) dv->dir = -dv->dir;
                }
            }
            int ri = dv->pend[best];
            double svc;
            if (bd == D) {
                svc = (r[ri].start ? 3 : 1) * SSD_LAT + CHUNK_MB / SSD_BW * 1000.0;
            } else {
                int c = cyl[r[ri].video], move = abs(c - dv->pos);
                double seek = move == 0 ? 0.0 : 1.0 + 0.04 * move;
                svc = seek + ROT + CHUNK_MB / HDD_BW * 1000.0 + (r[ri].start ? 2 * META : 0.0);
                dv->pos = c;
            }
            double done = dv->free_at + svc;
            double lat = done - r[ri].arr;
            dv->busy += svc;
            dv->free_at = done;
            dv->pend[best] = dv->pend[--dv->np];
            if (r[ri].start) sl[ns++] = lat; else { chunks++; if (lat > DEADLINE) late++; }
            served++;
        } else {
            int v = r[i].video, d;
            if (design == 1 && v < HOT) { d = D; onssd++; }
            else {
                int l1 = dev[rep1[v]].np + (dev[rep1[v]].free_at > r[i].arr), l2 = dev[rep2[v]].np + (dev[rep2[v]].free_at > r[i].arr);
                d = l2 < l1 ? rep2[v] : rep1[v];
            }
            if (dev[d].np == 0 && dev[d].free_at < r[i].arr) dev[d].free_at = r[i].arr;
            dev[d].pend[dev[d].np++] = i;
            i++;
        }
    }
    qsort(sl, (size_t)ns, sizeof(double), cmp_dbl);
    out->smed = sl[ns / 2]; out->sp95 = sl[(int)(ns * 0.95)]; out->sp99 = sl[(int)(ns * 0.99)];
    out->late = 100.0 * late / (chunks > 0 ? chunks : 1);
    double hb = 0; for (int d = 0; d < D; d++) hb += dev[d].busy;
    out->hddbusy = 100.0 * hb / (D * WINDOW * 1000.0);
    out->ssdshare = 100.0 * onssd / n;
    for (int d = 0; d <= D; d++) free(dev[d].pend);
    free(sl); free(r);
}

static double med(double a[RUNS]) {
    for (int i = 1; i < RUNS; i++) { double v = a[i]; int j = i - 1; while (j >= 0 && a[j] > v) { a[j + 1] = a[j]; j--; } a[j + 1] = v; }
    return a[RUNS / 2];
}

int main(void) {
    double h = 0;
    for (int v = 0; v < V; v++) h += 1.0 / (v + 1);
    double c = 0;
    for (int v = 0; v < V; v++) { c += 1.0 / (v + 1) / h; cdf[v] = c; }
    int streams_list[2] = {4000, 4500};
    const char *dn[2] = {"A: HDD only", "B: SSD tier + HDD"};
    printf("One server: %d HDDs (LOOK scheduler, 2 replicas, request 4 MiB) %s. Library %d videos, Zipf popularity, window %.0f s.\n",
           D, "and optionally one NVMe SSD", V, WINDOW);
    printf("Median of %d runs (different random seeds). All service times are modelled [A], so these are simulation results.\n\n", RUNS);
    printf("%-8s %-19s %10s %10s %10s %10s %10s %10s\n", "streams", "design", "start med", "start p95", "start p99", "late %", "HDD busy%", "on SSD %");
    for (int k = 0; k < 2; k++)
        for (int d = 0; d < 2; d++) {
            double a[6][RUNS];
            for (int s = 0; s < RUNS; s++) {
                rs = (unsigned long long)(2000 + 31 * s) * 0x9E3779B97F4A7C15ULL;
                Res o;
                run(streams_list[k], d, &o);
                a[0][s] = o.smed; a[1][s] = o.sp95; a[2][s] = o.sp99; a[3][s] = o.late; a[4][s] = o.hddbusy; a[5][s] = o.ssdshare;
            }
            printf("%-8d %-19s %8.1f ms %8.1f ms %8.1f ms %10.2f %10.1f %10.1f\n", streams_list[k], dn[d],
                   med(a[0]), med(a[1]), med(a[2]), med(a[3]), med(a[4]), med(a[5]));
        }
    printf("\nstart = time from request to the first 4 MiB chunk (includes 2 metadata reads); late = chunks later than %.0f s.\n", DEADLINE / 1000.0);
    printf("HDD busy above 100%% means a backlog is still growing when the window ends.\n");
    return 0;
}
