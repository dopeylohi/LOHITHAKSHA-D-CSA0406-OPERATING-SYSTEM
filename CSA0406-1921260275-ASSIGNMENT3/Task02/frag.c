#include <stdio.h>
#include <stdint.h>
#define NPROC 100
#define MAXSIZE (10u * 1024u * 1024u)
#define NSEED 5
static uint32_t s;
static uint32_t rnd(void) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
int main(void)
{
    const unsigned long ps[3] = {1024UL, 4096UL, 2097152UL};
    const char *nm[3] = {"1 KB", "4 KB", "2 MB"};
    double sum_avg[3] = {0, 0, 0}, sum_pct[3] = {0, 0, 0}, sum_pte[3] = {0, 0, 0};
    printf("%d processes, sizes uniform in 1..%u bytes [A], xorshift32 generator\n", NPROC, MAXSIZE);
    for (int seed = 1; seed <= NSEED; seed++) {
        unsigned long size[NPROC], total = 0;
        s = 2463534242u + (uint32_t)seed;
        for (int i = 0; i < NPROC; i++) { size[i] = rnd() % MAXSIZE + 1; total += size[i]; }
        printf("seed %d: total process size %lu bytes\n", seed, total);
        for (int k = 0; k < 3; k++) {
            unsigned long waste = 0, pages = 0;
            for (int i = 0; i < NPROC; i++) {
                unsigned long n = (size[i] + ps[k] - 1) / ps[k];
                pages += n;
                waste += n * ps[k] - size[i];
            }
            double avg = (double)waste / NPROC, pct = 100.0 * waste / (total + waste);
            printf("  page %s: pages (table entries) %lu, internal fragmentation %lu bytes, avg %.1f bytes/process, %.4f%% of allocated memory\n",
                   nm[k], pages, waste, avg, pct);
            sum_avg[k] += avg; sum_pct[k] += pct; sum_pte[k] += (double)pages;
        }
    }
    printf("MEAN over %d seeds:\n", NSEED);
    for (int k = 0; k < 3; k++)
        printf("  page %s: avg waste %.1f bytes/process (page/2 = %.1f), %.4f%% of allocated memory, %.0f table entries for 100 processes\n",
               nm[k], sum_avg[k] / NSEED, ps[k] / 2.0, sum_pct[k] / NSEED, sum_pte[k] / NSEED);
    return 0;
}
