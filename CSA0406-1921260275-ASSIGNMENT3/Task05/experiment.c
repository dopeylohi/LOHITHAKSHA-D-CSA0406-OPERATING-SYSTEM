#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#define MAXF 16
#define UNIVERSE 50     /* number of distinct pages [A] */
#define WSET 6          /* working-set size of the locality string [A] */
#define PHASE 1000      /* references per locality phase [A] */
#define NFAULT 10000    /* length of strings used for fault counts [A] */
#define NTIME 100000    /* length of strings used for timing (as in the question) */

static uint32_t rs;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

static int find(const int f[], int n, int page)
{
    for (int i = 0; i < n; i++) if (f[i] == page) return i;
    return -1;
}
static void show(const int f[], int used, int frames, int page, int fault)
{
    printf("  ref %2d : [", page);
    for (int j = 0; j < frames; j++) { if (j < used) printf(" %2d", f[j]); else printf("  -"); }
    printf(" ] %s\n", fault ? "FAULT" : "hit");
}
static int fifo(const int ref[], int len, int frames, int v)
{
    int f[MAXF], used = 0, next = 0, faults = 0;
    for (int i = 0; i < len; i++) {
        int fault = find(f, used, ref[i]) < 0;
        if (fault) {
            faults++;
            if (used < frames) f[used++] = ref[i];
            else { f[next] = ref[i]; next = (next + 1) % frames; }
        }
        if (v) show(f, used, frames, ref[i], fault);
    }
    return faults;
}
static int lru(const int ref[], int len, int frames, int v)
{
    int f[MAXF], last[MAXF], used = 0, faults = 0;
    for (int i = 0; i < len; i++) {
        int k = find(f, used, ref[i]), fault = k < 0;
        if (!fault) last[k] = i;
        else {
            faults++;
            if (used < frames) { f[used] = ref[i]; last[used] = i; used++; }
            else {
                int vic = 0;
                for (int j = 1; j < frames; j++) if (last[j] < last[vic]) vic = j;
                f[vic] = ref[i]; last[vic] = i;
            }
        }
        if (v) show(f, used, frames, ref[i], fault);
    }
    return faults;
}
static int clockalg(const int ref[], int len, int frames, int v)
{
    int f[MAXF], bit[MAXF], used = 0, hand = 0, faults = 0;
    for (int i = 0; i < len; i++) {
        int k = find(f, used, ref[i]), fault = k < 0;
        if (!fault) bit[k] = 1;
        else {
            faults++;
            if (used < frames) { f[used] = ref[i]; bit[used] = 1; used++; }
            else {
                while (bit[hand]) { bit[hand] = 0; hand = (hand + 1) % frames; }
                f[hand] = ref[i]; bit[hand] = 1; hand = (hand + 1) % frames;
            }
        }
        if (v) show(f, used, frames, ref[i], fault);
    }
    return faults;
}
/* Optimal, naive version (as in the question paper): scans forward for every frame at every fault */
static int optimal_naive(const int ref[], int len, int frames, int v)
{
    int f[MAXF], used = 0, faults = 0;
    for (int i = 0; i < len; i++) {
        int fault = find(f, used, ref[i]) < 0;
        if (fault) {
            faults++;
            if (used < frames) f[used++] = ref[i];
            else {
                int victim = 0, farthest = -1;
                for (int j = 0; j < frames; j++) {
                    int next = len;
                    for (int t = i + 1; t < len; t++) if (ref[t] == f[j]) { next = t; break; }
                    if (next > farthest) { farthest = next; victim = j; }
                }
                f[victim] = ref[i];
            }
        }
        if (v) show(f, used, frames, ref[i], fault);
    }
    return faults;
}
/* Optimal, fast version: next-use index of every reference is computed once (same victims as the naive one) */
static int optimal_fast(const int ref[], int len, int frames)
{
    int *nxt = malloc(sizeof(int) * (size_t)len);
    int seen[UNIVERSE + 8], f[MAXF], fn[MAXF], used = 0, faults = 0;
    if (!nxt) return -1;
    for (int p = 0; p < UNIVERSE + 8; p++) seen[p] = len;
    for (int i = len - 1; i >= 0; i--) { nxt[i] = seen[ref[i]]; seen[ref[i]] = i; }
    for (int i = 0; i < len; i++) {
        int k = find(f, used, ref[i]);
        if (k >= 0) { fn[k] = nxt[i]; continue; }
        faults++;
        if (used < frames) { f[used] = ref[i]; fn[used] = nxt[i]; used++; }
        else {
            int vic = 0;
            for (int j = 1; j < frames; j++) if (fn[j] > fn[vic]) vic = j;
            f[vic] = ref[i]; fn[vic] = nxt[i];
        }
    }
    free(nxt);
    return faults;
}
static void gen_random(int *r, int n, uint32_t seed)
{
    rs = seed;
    for (int i = 0; i < n; i++) r[i] = (int)(rnd() % UNIVERSE);
}
static void gen_locality(int *r, int n, uint32_t seed)
{
    int ws[WSET];
    rs = seed;
    for (int i = 0; i < n; i++) {
        if (i % PHASE == 0)
            for (int k = 0; k < WSET; ) {
                int p = (int)(rnd() % UNIVERSE), dup = 0;
                for (int j = 0; j < k; j++) if (ws[j] == p) dup = 1;
                if (!dup) ws[k++] = p;
            }
        r[i] = (rnd() % 10 < 9) ? ws[rnd() % WSET] : (int)(rnd() % UNIVERSE);   /* 90% inside working set */
    }
}
static double now_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}
static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "csv";
    static int loc[NTIME], rnd_s[NTIME];
    gen_locality(loc, NTIME, 12345u);
    gen_random(rnd_s, NTIME, 54321u);

    if (!strcmp(mode, "csv")) {
        printf("string,frames,FIFO,LRU,Clock,Optimal\n");
        for (int s = 0; s < 2; s++) {
            const int *r = s ? rnd_s : loc;
            for (int fr = 3; fr <= 10; fr++)
                printf("%s,%d,%d,%d,%d,%d\n", s ? "random" : "locality", fr, fifo(r, NFAULT, fr, 0),
                       lru(r, NFAULT, fr, 0), clockalg(r, NFAULT, fr, 0), optimal_fast(r, NFAULT, fr));
        }
    } else if (!strcmp(mode, "trace")) {
        int ref[] = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1};
        printf("FIFO, 3 frames\n");  int a = fifo(ref, 20, 3, 1);
        printf("LRU, 3 frames\n");   int b = lru(ref, 20, 3, 1);
        printf("Optimal, 3 frames\n"); int c = optimal_naive(ref, 20, 3, 1);
        printf("Clock, 3 frames\n"); int d = clockalg(ref, 20, 3, 1);
        printf("TOTAL faults, 3 frames: FIFO %d, LRU %d, Optimal %d, Clock %d\n", a, b, c, d);
    } else if (!strcmp(mode, "check")) {
        int b[] = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
        printf("Belady string: FIFO 3 frames %d, 4 frames %d ; LRU 3 frames %d, 4 frames %d ; Optimal 3 frames %d, 4 frames %d\n",
               fifo(b, 12, 3, 0), fifo(b, 12, 4, 0), lru(b, 12, 3, 0), lru(b, 12, 4, 0), optimal_naive(b, 12, 3, 0), optimal_naive(b, 12, 4, 0));
        for (int s = 0; s < 2; s++) {
            const int *r = s ? rnd_s : loc;
            int lru_ok = 1, fifo_up = 0, opt_ok = 1;
            for (int fr = 3; fr < 10; fr++) {
                if (lru(r, NFAULT, fr + 1, 0) > lru(r, NFAULT, fr, 0)) lru_ok = 0;
                if (fifo(r, NFAULT, fr + 1, 0) > fifo(r, NFAULT, fr, 0)) fifo_up++;
            }
            for (int fr = 3; fr <= 10; fr++) if (optimal_fast(r, NFAULT, fr) != optimal_naive(r, NFAULT, fr, 0)) opt_ok = 0;
            printf("%s string, frames 3..10: LRU faults never increase with more frames: %s ; FIFO increases at %d of 7 steps ; optimal_fast == optimal_naive: %s\n",
                   s ? "random  " : "locality", lru_ok ? "YES" : "NO", fifo_up, opt_ok ? "YES" : "NO");
        }
    } else if (!strcmp(mode, "timing")) {
        const int fr = 8;   /* [A] */
        const char *nm[5] = {"FIFO", "LRU", "Clock", "Optimal(fast)", "Optimal(naive)"};
        printf("100000 references, %d frames [A]; 5 runs each, times in ms; last column = median [M]\n", fr);
        for (int s = 0; s < 2; s++) {
            const int *r = s ? rnd_s : loc;
            for (int a = 0; a < 5; a++) {
                double t[5]; int faults = 0;
                for (int k = 0; k < 5; k++) {
                    double t0 = now_ms();
                    switch (a) {
                    case 0: faults = fifo(r, NTIME, fr, 0); break;
                    case 1: faults = lru(r, NTIME, fr, 0); break;
                    case 2: faults = clockalg(r, NTIME, fr, 0); break;
                    case 3: faults = optimal_fast(r, NTIME, fr); break;
                    default: faults = optimal_naive(r, NTIME, fr, 0); break;
                    }
                    t[k] = now_ms() - t0;
                }
                double u[5]; memcpy(u, t, sizeof t); qsort(u, 5, sizeof(double), cmpd);
                printf("%-8s %-15s faults %6d  runs %.2f %.2f %.2f %.2f %.2f  median %.2f ms\n", s ? "random" : "locality", nm[a], faults, t[0], t[1], t[2], t[3], t[4], u[2]);
            }
        }
    } else { fprintf(stderr, "modes: csv | check | trace | timing\n"); return 1; }
    return 0;
}
