#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Workload (ASSUMED): 3 long jobs (burst 30) at t=0 and 12 short jobs (burst 2) arriving
   at t = 2, 5, 8, ..., 35.  Slice-based simulation like rr.c (a slice is not interrupted). */
#define NJ 15
#define NLONG 3
static int arrival[NJ], burst[NJ], left_[NJ], finish[NJ];
enum { M_FCFS, M_RR4, M_SJF, M_MLFQ };
static const int QUANTA[3] = {2, 4, 8};

static void init(void)
{
    for (int i = 0; i < NJ; i++) {
        arrival[i] = i < NLONG ? 0 : 2 + 3 * (i - NLONG);
        burst[i] = i < NLONG ? 30 : 2;
        left_[i] = burst[i]; finish[i] = -1;
    }
}

static int simulate(int mode)          /* returns number of dispatches (slices) */
{
    int qs[3][512], hd[3] = {0}, tl[3] = {0}, added[NJ] = {0};
    int t = 0, done = 0, slices = 0;
    while (done < NJ) {
        for (int i = 0; i < NJ; i++)
            if (!added[i] && arrival[i] <= t) { qs[0][tl[0]++] = i; added[i] = 1; }
        int p = -1, lvl = 0, quantum = 1 << 20;
        if (mode == M_SJF) {                                   /* O(n) scan for shortest job */
            for (int k = hd[0]; k < tl[0]; k++) {
                int j = qs[0][k];
                if (left_[j] > 0 && (p < 0 || burst[j] < burst[p])) p = j;
            }
            if (p >= 0) {                                      /* remove p from the queue */
                for (int k = hd[0]; k < tl[0]; k++)
                    if (qs[0][k] == p) { memmove(&qs[0][k], &qs[0][k + 1], (tl[0] - k - 1) * sizeof(int)); tl[0]--; break; }
            }
        } else {
            for (lvl = 0; lvl < 3; lvl++) if (hd[lvl] != tl[lvl]) break;
            if (lvl < 3) p = qs[lvl][hd[lvl]++];
            if (mode == M_RR4) quantum = 4;
            if (mode == M_MLFQ) quantum = QUANTA[lvl];
        }
        if (p < 0) { t++; continue; }                          /* idle */
        int run = left_[p] < quantum ? left_[p] : quantum;
        t += run; left_[p] -= run; slices++;
        for (int i = 0; i < NJ; i++)
            if (!added[i] && arrival[i] <= t) { qs[0][tl[0]++] = i; added[i] = 1; }
        if (left_[p] > 0) {
            int nl = 0;
            if (mode == M_MLFQ) nl = lvl < 2 ? lvl + 1 : 2;    /* used its quantum: demote */
            qs[nl][tl[nl]++] = p;
        } else { finish[p] = t; done++; }
    }
    return slices;
}

static void report(const char *title, int mode)
{
    init();
    int slices = simulate(mode);
    double ws = 0, wl = 0; int maxl = 0;
    for (int i = 0; i < NJ; i++) {
        int w = finish[i] - arrival[i] - burst[i];
        if (i < NLONG) { wl += w; if (w > maxl) maxl = w; } else ws += w;
    }
    printf("%-26s avg WT short = %6.2f   avg WT long = %6.2f   max WT long = %3d   dispatches = %3d\n",
           title, ws / (NJ - NLONG), wl / NLONG, maxl, slices);
}

/* ---- cost of one scheduling decision: SJF scan O(n) vs MLFQ queue-head lookup O(1) ---- */
static double now(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return ts.tv_sec + ts.tv_nsec / 1e9; }
static volatile long sink;

static void pick_bench(int n)
{
    int *b = malloc(n * sizeof *b);
    int cap = n + 1, *q[3], hd[3] = {0, 0, 0}, cnt[3] = {n, 0, 0};
    for (int l = 0; l < 3; l++) q[l] = malloc(cap * sizeof(int));
    for (int i = 0; i < n; i++) { b[i] = (i * 7919) % 1000 + 1; q[0][i] = i; }
    long reps_sjf = 20000000L / n, reps_mlfq = 20000000L;

    double t0 = now();
    for (long r = 0; r < reps_sjf; r++) {
        int best = 0;
        for (int i = 1; i < n; i++) if (b[i] < b[best]) best = i;   /* O(n) scan */
        sink += best;
        b[(r * 31) % n] += (int)(r & 1);                            /* keep the loop honest */
    }
    double sjf_ns = (now() - t0) * 1e9 / reps_sjf;

    t0 = now();
    for (long r = 0; r < reps_mlfq; r++) {
        int l = 0;
        while (l < 3 && cnt[l] == 0) l++;                           /* first non-empty queue */
        int j = q[l][hd[l]];                                        /* O(1) pop from head */
        hd[l] = (hd[l] + 1) % cap; cnt[l]--;
        int nl = l < 2 ? l + 1 : 2;
        q[nl][(hd[nl] + cnt[nl]) % cap] = j; cnt[nl]++;             /* O(1) push to tail */
        sink += j;
    }
    double mlfq_ns = (now() - t0) * 1e9 / reps_mlfq;
    printf("n=%6d ready jobs: SJF decision = %10.1f ns   MLFQ decision = %6.1f ns\n", n, sjf_ns, mlfq_ns);
    free(b); for (int l = 0; l < 3; l++) free(q[l]);
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "pick")) {
        int ns[] = {10, 100, 1000, 10000};
        for (int i = 0; i < 4; i++) pick_bench(ns[i]);
        return 0;
    }
    printf("3 long jobs (burst 30, t=0) + 12 short jobs (burst 2, t=2,5,...,35)\n");
    report("FCFS", M_FCFS);
    report("SJF (non-preemptive)", M_SJF);
    report("Round Robin q=4", M_RR4);
    report("MLFQ quanta 2/4/8", M_MLFQ);
    return 0;
}
