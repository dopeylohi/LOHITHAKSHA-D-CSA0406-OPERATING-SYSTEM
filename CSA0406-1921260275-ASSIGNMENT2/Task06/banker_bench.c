#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* The same safety algorithm as banker.c, with n processes and m resource types chosen at run time.
   best  : plenty available, every process can finish in the first pass            -> about n x m steps
   worst : only ONE process can finish per pass (a chain), so n passes are needed -> about n x n x m steps */
static int safe(int n, int m, const int *avail, const int *maxm, const int *alloc,
                int *seq, int *finish, int *work)
{
    memcpy(work, avail, (size_t)m * sizeof(int));
    memset(finish, 0, (size_t)n * sizeof(int));
    int count = 0;
    while (count < n) {
        int found = 0;
        for (int p = 0; p < n; p++) {
            if (finish[p]) continue;
            int ok = 1;
            for (int r = 0; r < m; r++)
                if (maxm[p * m + r] - alloc[p * m + r] > work[r]) { ok = 0; break; }
            if (ok) {
                for (int r = 0; r < m; r++) work[r] += alloc[p * m + r];
                finish[p] = 1;
                seq[count++] = p;
                found = 1;
            }
        }
        if (!found) return 0;
    }
    return 1;
}

static void build(int worst, int n, int m, int *avail, int *maxm, int *alloc)
{
    memset(avail, 0, (size_t)m * sizeof(int));
    memset(alloc, 0, (size_t)n * m * sizeof(int));
    memset(maxm, 0, (size_t)n * m * sizeof(int));
    if (!worst) {                                   /* everything fits at once */
        for (int r = 0; r < m; r++) avail[r] = 1000000;
        for (int p = 0; p < n; p++)
            for (int r = 0; r < m; r++) { alloc[p * m + r] = 1; maxm[p * m + r] = 2; }
        return;
    }
    avail[m - 1] = 1;                               /* bottleneck resource is the LAST one */
    for (int p = 0; p < n; p++) {
        alloc[p * m + (m - 1)] = 1;
        maxm[p * m + (m - 1)] = 1 + (n - p);        /* need = n - p : process n-1 needs 1, n-2 needs 2 ... */
    }
}

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "check")) {
        /* 1. the given 5 x 3 state: must give P1 P3 P4 P0 P2 */
        int av[3] = {3, 3, 2};
        int mx[15] = {7, 5, 3, 3, 2, 2, 9, 0, 2, 2, 2, 2, 4, 3, 3};
        int al[15] = {0, 1, 0, 2, 0, 0, 3, 0, 2, 2, 1, 1, 0, 0, 2};
        int seq[5], fin[5], wk[3];
        int s = safe(5, 3, av, mx, al, seq, fin, wk);
        printf("given state: safe=%d sequence:", s);
        for (int i = 0; i < 5; i++) printf(" P%d", seq[i]);
        printf("\n");
        /* 2. the worst-case chain for n = 10 must finish in the order P9 P8 ... P0 */
        int n = 10, m = 3;
        int *a = malloc(m * sizeof(int)), *x = malloc(n * m * sizeof(int)), *l = malloc(n * m * sizeof(int));
        int *sq = malloc(n * sizeof(int)), *f = malloc(n * sizeof(int)), *w = malloc(m * sizeof(int));
        build(1, n, m, a, x, l);
        s = safe(n, m, a, x, l, sq, f, w);
        printf("worst-case chain n=10: safe=%d sequence:", s);
        for (int i = 0; i < n; i++) printf(" P%d", sq[i]);
        printf("\n");
        build(0, n, m, a, x, l);
        s = safe(n, m, a, x, l, sq, f, w);
        printf("best-case state  n=10: safe=%d sequence:", s);
        for (int i = 0; i < n; i++) printf(" P%d", sq[i]);
        printf("\n");
        return 0;
    }
    if (argc != 4) { fprintf(stderr, "usage: %s check | best|worst n m\n", argv[0]); return 1; }
    int worst = !strcmp(argv[1], "worst");
    int n = atoi(argv[2]), m = atoi(argv[3]);
    int *a = malloc(m * sizeof(int)), *x = malloc((size_t)n * m * sizeof(int)), *l = malloc((size_t)n * m * sizeof(int));
    int *sq = malloc(n * sizeof(int)), *f = malloc(n * sizeof(int)), *w = malloc(m * sizeof(int));
    build(worst, n, m, a, x, l);
    long steps = worst ? (long)n * n * m / 2 : (long)n * m;       /* rough work per call */
    long reps = 100000000L / (steps > 0 ? steps : 1);
    if (reps < 5) reps = 5;
    long okcount = 0;
    double t0 = now();
    for (long i = 0; i < reps; i++) okcount += safe(n, m, a, x, l, sq, f, w);
    double us = (now() - t0) * 1e6 / reps;
    printf("case=%-5s n=%5d m=%3d reps=%ld time_per_check=%.3f us %s\n", argv[1], n, m, reps, us,
           okcount == reps ? "safe" : "UNSAFE?");
    return 0;
}
