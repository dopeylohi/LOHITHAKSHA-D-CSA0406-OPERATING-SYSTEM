#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>
static uint32_t rs = 2463534243u;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000.0 + t.tv_nsec / 1e6; }
int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "seq";
    size_t mb = argc > 2 ? (size_t)atol(argv[2]) : 64;
    long ps = sysconf(_SC_PAGESIZE);
    size_t pages = (mb << 20) / (size_t)ps;
    volatile char *p = mmap(NULL, mb << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    uint32_t *ord = malloc(pages * sizeof(uint32_t));
    if (p == MAP_FAILED || !ord) { perror("alloc"); return 1; }
    for (size_t i = 0; i < pages; i++) ord[i] = (uint32_t)i;
    if (!strcmp(mode, "rand"))
        for (size_t i = pages - 1; i > 0; i--) { size_t j = rnd() % (i + 1); uint32_t t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
    printf("mode %s, %zu MB = %zu pages of %ld bytes\n", mode, mb, pages, ps);
    for (int pass = 1; pass <= 2; pass++) {
        struct rusage a, b;
        getrusage(RUSAGE_SELF, &a);
        double t0 = now_ms();
        for (size_t i = 0; i < pages; i++) p[(size_t)ord[i] * (size_t)ps] = 1;
        double ms = now_ms() - t0;
        getrusage(RUSAGE_SELF, &b);
        printf("pass %d (%s): minor faults %ld, major faults %ld, time %.2f ms, %.1f ns per page\n", pass,
               pass == 1 ? "first touch" : "second touch", b.ru_minflt - a.ru_minflt, b.ru_majflt - a.ru_majflt, ms, ms * 1e6 / (double)pages);
    }
    return 0;
}
