#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/resource.h>
static uint32_t rs = 88172645u;
static uint32_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000.0 + t.tv_nsec / 1e6; }
static void cg_info(void)
{
    char line[512], path[600], val[128] = "?", sw[128] = "?";
    FILE *f = fopen("/proc/self/cgroup", "r");
    if (!f) return;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "0::", 3)) continue;
        line[strcspn(line, "\n")] = 0;
        snprintf(path, sizeof path, "/sys/fs/cgroup%s/memory.max", line + 3);
        FILE *g = fopen(path, "r");
        if (g) { if (fgets(val, sizeof val, g)) val[strcspn(val, "\n")] = 0; fclose(g); }
        snprintf(path, sizeof path, "/sys/fs/cgroup%s/memory.swap.max", line + 3);
        g = fopen(path, "r");
        if (g) { if (fgets(sw, sizeof sw, g)) sw[strcspn(sw, "\n")] = 0; fclose(g); }
        fprintf(stderr, "# cgroup %s : memory.max=%s memory.swap.max=%s\n", line + 3, val, sw);
    }
    fclose(f);
}
int main(int argc, char **argv)
{
    size_t mb = argc > 1 ? (size_t)atol(argv[1]) : 64;
    const char *mode = argc > 2 ? argv[2] : "seq";
    long ps = sysconf(_SC_PAGESIZE);
    size_t pages = (mb << 20) / (size_t)ps;
    double pass_ms[3];
    long minf = 0, majf = 0;
    cg_info();
    volatile char *p = mmap(NULL, mb << 20, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    uint32_t *ord = malloc(pages * sizeof(uint32_t));
    if (p == MAP_FAILED || !ord) { perror("alloc"); return 1; }
    double t0 = now_ms();
    for (size_t i = 0; i < pages; i++) p[i * (size_t)ps] = 1;
    double first = now_ms() - t0;
    for (int pass = 0; pass < 3; pass++) {
        for (size_t i = 0; i < pages; i++) ord[i] = (uint32_t)i;
        if (!strcmp(mode, "rand"))
            for (size_t i = pages - 1; i > 0; i--) { size_t j = rnd() % (i + 1); uint32_t t = ord[i]; ord[i] = ord[j]; ord[j] = t; }
        struct rusage a, b;
        getrusage(RUSAGE_SELF, &a);
        t0 = now_ms();
        for (size_t i = 0; i < pages; i++) p[(size_t)ord[i] * (size_t)ps]++;
        pass_ms[pass] = now_ms() - t0;
        getrusage(RUSAGE_SELF, &b);
        minf += b.ru_minflt - a.ru_minflt;
        majf += b.ru_majflt - a.ru_majflt;
    }
    printf("%zu,%s,%.1f,%.1f,%.1f,%.1f,%ld,%ld\n", mb, mode, first, pass_ms[0], pass_ms[1], pass_ms[2], minf, majf);
    return 0;
}
