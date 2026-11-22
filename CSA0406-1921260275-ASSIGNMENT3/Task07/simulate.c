#include <stdio.h>
#include <stdint.h>
/* Page-replacement simulation of the edge device. 1 simulated page = 1 MB (256 real 4 KB pages) [A]. */
#define RAM 2048        /* 2 GB RAM, no swap */
#define OSRES 300       /* kernel + OS + libraries reserve [A] */
#define RT_N 16         /* real-time control task working set: 16 MB [A] */
#define ML_N 1536       /* machine-learning task: 1.5 GB (given), read-only model mapped from flash */
#define LOG_N 64        /* logging task buffer ring: 64 MB [A] */
#define T4_N 500        /* possible fourth task: 500 MB (given) */
#define TICKS 30000
#define WARM 3000
#define RT_PERIOD 300   /* the control task runs every 300 ticks [A]; between runs its pages are not used */
#define ML_STEP 8       /* the ML task reads 8 consecutive model pages per tick, cyclic [A] */
#define NP (RT_N + ML_N + LOG_N + T4_N)
static uint32_t rs = 2463534242u;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static int resident[NP], pinned[NP];
static long last[NP];
static int used, avail, npages;
static long now;
static long faults[4], rt_act_faults, rt_max_act;
static void access_page(int page, int task, int tick)
{
    if (pinned[page]) return;
    if (resident[page]) { last[page] = now++; return; }
    if (tick >= WARM) { faults[task]++; if (task == 0) rt_act_faults++; }
    if (used < avail) used++;
    else {
        int v = -1;
        for (int i = 0; i < npages; i++) if (resident[i] && !pinned[i] && (v < 0 || last[i] < last[v])) v = i;   /* LRU victim */
        resident[v] = 0;
    }
    resident[page] = 1; last[page] = now++;
}
static void run(int t4, int lock, const char *name)
{
    npages = RT_N + ML_N + LOG_N + (t4 ? T4_N : 0);
    for (int i = 0; i < NP; i++) { resident[i] = 0; pinned[i] = 0; last[i] = 0; }
    for (int k = 0; k < 4; k++) faults[k] = 0;
    used = 0; now = 1; rt_act_faults = 0; rt_max_act = 0; rs = 2463534242u;
    avail = RAM - OSRES - (lock ? RT_N : 0);
    if (lock) for (int i = 0; i < RT_N; i++) { pinned[i] = 1; resident[i] = 1; }
    int ml_pos = 0, log_pos = 0;
    for (int tick = 0; tick < TICKS; tick++) {
        if (tick % RT_PERIOD == 0) {
            rt_act_faults = 0;
            for (int i = 0; i < RT_N; i++) access_page(i, 0, tick);
            if (tick >= WARM && rt_act_faults > rt_max_act) rt_max_act = rt_act_faults;
        }
        for (int k = 0; k < ML_STEP; k++) { access_page(RT_N + ml_pos, 1, tick); ml_pos = (ml_pos + 1) % ML_N; }
        access_page(RT_N + ML_N + log_pos, 2, tick); log_pos = (log_pos + 1) % LOG_N;
        if (t4) for (int k = 0; k < 4; k++) access_page(RT_N + ML_N + LOG_N + (int)(rnd() % T4_N), 3, tick);
    }
    double sweeps = (double)(TICKS - WARM) * ML_STEP / ML_N;
    printf("%-34s demand %4d MB, frames for non-pinned pages %4d | faults after warm-up: RT %ld (max %ld in one activation), ML %ld (%.1f per sweep), log %ld, task4 %ld\n",
           name, npages - (lock ? RT_N : 0), avail, faults[0], rt_max_act, faults[1], faults[1] / sweeps, faults[2], faults[3]);
}
int main(void)
{
    printf("RAM %d MB, OS reserve %d MB, %d ticks (%d warm-up), RT period %d ticks, 1 page = 1 MB [A]\n", RAM, OSRES, TICKS, WARM, RT_PERIOD);
    run(0, 1, "S1: 3 tasks, RT pages locked");
    run(0, 0, "S2: 3 tasks, RT pages NOT locked");
    run(1, 1, "S3: 4 tasks, RT pages locked");
    run(1, 0, "S4: 4 tasks, RT pages NOT locked");
    return 0;
}
