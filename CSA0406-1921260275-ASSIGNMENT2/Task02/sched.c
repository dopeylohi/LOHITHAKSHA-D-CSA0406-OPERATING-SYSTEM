#include <stdio.h>

#define N 4
#define MAXT 200
enum { FCFS, SJF, SRTF, PRIO, RR };

static const char *name[N] = {"P1", "P2", "P3", "P4"};
static int arrival[N] = {0, 1, 2, 3};
static int burst[N]   = {8, 4, 9, 5};
static int prio[N]    = {2, 3, 1, 4};      /* ASSUMED: lower number = higher priority */
static int tl[MAXT], T;                    /* timeline: process id per time unit, -1 = idle */
static int start_t[N], finish_t[N];

static int better(int algo, int a, int b, const int left[N])
{
    int ka, kb;
    switch (algo) {
    case FCFS: ka = arrival[a]; kb = arrival[b]; break;
    case SJF:  ka = burst[a];   kb = burst[b];   break;
    case SRTF: ka = left[a];    kb = left[b];    break;
    default:   ka = prio[a];    kb = prio[b];    break;
    }
    if (ka != kb) return ka < kb;
    if (arrival[a] != arrival[b]) return arrival[a] < arrival[b];
    return a < b;
}

static void simulate(int algo, int q)
{
    int left[N], t = 0, done = 0;
    for (int i = 0; i < N; i++) { left[i] = burst[i]; start_t[i] = -1; }
    if (algo == RR) {
        int queue[256], head = 0, tail = 0, added[N] = {0};
        while (done < N) {
            for (int i = 0; i < N; i++)
                if (!added[i] && arrival[i] <= t) { queue[tail++] = i; added[i] = 1; }
            if (head == tail) { tl[t++] = -1; continue; }          /* idle */
            int p = queue[head++];
            int run = left[p] < q ? left[p] : q;
            if (start_t[p] < 0) start_t[p] = t;
            for (int k = 0; k < run; k++) tl[t++] = p;
            left[p] -= run;
            for (int i = 0; i < N; i++)
                if (!added[i] && arrival[i] <= t) { queue[tail++] = i; added[i] = 1; }
            if (left[p] > 0) queue[tail++] = p;
            else { finish_t[p] = t; done++; }
        }
    } else {
        int cur = -1;
        while (done < N) {
            if (algo == SRTF || cur < 0) {
                cur = -1;
                for (int i = 0; i < N; i++)
                    if (arrival[i] <= t && left[i] > 0 && (cur < 0 || better(algo, i, cur, left)))
                        cur = i;
            }
            if (cur < 0) { tl[t++] = -1; continue; }
            tl[t++] = cur;
            if (start_t[cur] < 0) start_t[cur] = t - 1;
            if (--left[cur] == 0) { finish_t[cur] = t; done++; cur = -1; }
        }
    }
    T = t;
}

static double s_wt[8], s_tat[8], s_rt[8]; static int s_sw[8]; static char s_name[8][64]; static int ns;

static void report(const char *title, int full)
{
    int segs = 0;
    if (full) {
        printf("%s\nGantt:", title);
    }
    for (int t = 0; t < T; ) {
        int p = tl[t], e = t;
        while (e < T && tl[e] == p) e++;
        if (full) printf(" %s(%d-%d)", p < 0 ? "idle" : name[p], t, e);
        segs++;
        t = e;
    }
    double wt = 0, tat = 0, rt = 0;
    if (full) printf("\nProc AT BT PR  Fin TAT  WT  RT\n");
    for (int i = 0; i < N; i++) {
        int ta = finish_t[i] - arrival[i], w = ta - burst[i], r = start_t[i] - arrival[i];
        wt += w; tat += ta; rt += r;
        if (full) printf("%-4s %2d %2d %2d %4d %3d %3d %3d\n", name[i], arrival[i], burst[i], prio[i],
                         finish_t[i], ta, w, r);
    }
    if (full) printf("Average: TAT = %.2f, WT = %.2f, RT = %.2f, context switches = %d\n\n",
                     tat / N, wt / N, rt / N, segs - 1);
    snprintf(s_name[ns], sizeof s_name[ns], "%s", title);
    s_wt[ns] = wt / N; s_tat[ns] = tat / N; s_rt[ns] = rt / N; s_sw[ns] = segs - 1; ns++;
}

int main(void)
{
    simulate(FCFS, 0); report("FCFS", 1);
    simulate(SJF, 0);  report("SJF (non-preemptive)", 1);
    simulate(SRTF, 0); report("SRTF (preemptive SJF)", 1);
    simulate(PRIO, 0); report("Priority (non-preemptive, assumed priorities)", 1);
    simulate(RR, 3);   report("Round Robin q=3", 1);
    simulate(RR, 1);   report("Round Robin q=1", 0);
    simulate(RR, 5);   report("Round Robin q=5", 0);
    simulate(RR, 20);  report("Round Robin q=20", 0);
    printf("SUMMARY (jobs P1..P4: AT 0,1,2,3  BT 8,4,9,5  priority 2,3,1,4)\n");
    printf("%-48s %7s %7s %7s %6s\n", "Algorithm", "avgTAT", "avgWT", "avgRT", "switch");
    for (int i = 0; i < ns; i++)
        printf("%-48s %7.2f %7.2f %7.2f %6d\n", s_name[i], s_tat[i], s_wt[i], s_rt[i], s_sw[i]);
    return 0;
}
