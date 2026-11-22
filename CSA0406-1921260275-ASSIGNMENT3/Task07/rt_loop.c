#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#ifndef MADV_PAGEOUT
#define MADV_PAGEOUT 21
#endif
#define MAXIT 20000
static double ms_between(const struct timespec *a, const struct timespec *b) { return (b->tv_sec - a->tv_sec) * 1000.0 + (b->tv_nsec - a->tv_nsec) / 1e6; }
static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
static void add_ms(struct timespec *t, double ms)
{
    long long ns = (long long)(ms * 1e6) + t->tv_nsec;
    t->tv_sec += ns / 1000000000LL; t->tv_nsec = ns % 1000000000LL;
}
int main(int argc, char **argv)
{
    int lock = argc > 1 && !strcmp(argv[1], "lock");
    const char *pressure = argc > 2 ? argv[2] : "none";          /* none | pageout | hog (hog = another process creates the pressure) */
    size_t mb = argc > 3 ? (size_t)atol(argv[3]) : 32;
    double secs = argc > 4 ? atof(argv[4]) : 10.0;
    double period = 50.0, deadline = 10.0;                        /* ms [A] */
    long ps = sysconf(_SC_PAGESIZE);
    size_t bytes = mb << 20, pages = bytes / (size_t)ps;
    static double resp[MAXIT];
    int n = 0, misses = 0;
    long skipped = 0;
    if (lock && mlockall(MCL_CURRENT | MCL_FUTURE) != 0) { fprintf(stderr, "mlockall failed: %s (run with sudo)\n", strerror(errno)); return 2; }
    volatile char *buf = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buf == MAP_FAILED) { perror("mmap"); return 1; }
    for (size_t i = 0; i < pages; i++) buf[i * (size_t)ps] = 1;   /* pre-fault: all pages resident before the loop starts */
    struct rusage r0, r1;
    struct timespec start, next, now, fin;
    getrusage(RUSAGE_SELF, &r0);
    clock_gettime(CLOCK_MONOTONIC, &start);
    next = start;
    for (;;) {
        add_ms(&next, period);
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (ms_between(&start, &now) > secs * 1000.0 || n >= MAXIT) break;
        for (size_t i = 0; i < pages; i++) buf[i * (size_t)ps]++;          /* the time-critical work: touch the whole working set */
        clock_gettime(CLOCK_MONOTONIC, &fin);
        resp[n] = ms_between(&next, &fin);                                  /* response time = finish - scheduled release */
        if (resp[n] > deadline) misses++;
        n++;
        if (!strcmp(pressure, "pageout")) madvise((void *)buf, bytes, MADV_PAGEOUT);   /* ask the kernel to reclaim the pages while the task sleeps */
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (ms_between(&next, &now) > period) { skipped += (long)(ms_between(&next, &now) / period); next = now; }
    }
    getrusage(RUSAGE_SELF, &r1);
    double sum = 0, mx = 0;
    for (int i = 0; i < n; i++) { sum += resp[i]; if (resp[i] > mx) mx = resp[i]; }
    qsort(resp, (size_t)n, sizeof(double), cmpd);
    double p99 = n ? resp[(int)(0.99 * (n - 1))] : 0;
    printf("%s,%s,%d,%.3f,%.3f,%.3f,%d,%ld,%ld,%ld\n", lock ? "lock" : "nolock", pressure, n, mx, p99, n ? sum / n : 0, misses, skipped,
           r1.ru_minflt - r0.ru_minflt, r1.ru_majflt - r0.ru_majflt);
    return 0;
}
