#include <stdio.h>
#include <stdint.h>
#define NSEG 20
#define MEM_KB 1024
#define NSEED 5
static uint32_t s;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
int main(void)
{
    double sum_ext = 0;
    printf("memory %d KB, %d segments of 4..64 KB (whole KB) [A], first-fit, remove 10 random segments, 5 seeds\n", MEM_KB, NSEG);
    for (int seed = 1; seed <= NSEED; seed++) {
        unsigned size[NSEG], start[NSEG], total = 0, alive[NSEG];
        s = 2463534242u + (uint32_t)seed;
        for (int i = 0; i < NSEG; i++) { size[i] = 4 + rnd() % 61; start[i] = total; total += size[i]; alive[i] = 1; }
        if (total > MEM_KB) { printf("seed %d: segments do not fit, skipped\n", seed); continue; }
        int removed = 0;
        while (removed < 10) { unsigned k = rnd() % NSEG; if (alive[k]) { alive[k] = 0; removed++; } }
        unsigned holes[NSEG + 1], nh = 0, prev = 0, freekb = 0, largest = 0, frames = 0, waste = 0;
        for (int i = 0; i < NSEG; i++) {
            if (!alive[i]) continue;
            if (start[i] > prev) holes[nh++] = start[i] - prev;
            prev = start[i] + size[i];
        }
        if (MEM_KB > prev) holes[nh++] = MEM_KB - prev;
        for (unsigned h = 0; h < nh; h++) { freekb += holes[h]; if (holes[h] > largest) largest = holes[h]; frames += holes[h] / 4; }
        for (int i = 0; i < NSEG; i++) waste += (size[i] % 4) ? 4 - size[i] % 4 : 0;
        double ext = 100.0 * (1.0 - (double)largest / freekb);
        sum_ext += ext;
        printf("seed %d: loaded %u KB, after removing 10: %u holes [", seed, total, nh);
        for (unsigned h = 0; h < nh; h++) printf("%s%u", h ? " " : "", holes[h]);
        printf("] KB, free %u KB, largest hole %u KB, external fragmentation %.1f%%\n", freekb, largest, ext);
        unsigned req = largest + 1;
        if (req <= freekb) {
            printf("   test request of %u KB (= largest hole + 1 KB, chosen to exceed it): segmentation %s (largest hole %u KB < %u KB, total free %u KB); paging needs %u pages of 4 KB, free frames %u -> %s\n",
                   req, "FAILS", largest, req, freekb, (req + 3) / 4, frames, ((req + 3) / 4 <= frames) ? "fits" : "does not fit");
        }
        printf("   paging, same 20 segments with 4 KB pages: internal fragmentation %u KB in total (KB-sized segments, [C])\n", waste);
    }
    printf("MEAN external fragmentation over %d seeds: %.1f%%\n", NSEED, sum_ext / NSEED);
    return 0;
}
