#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/resource.h>

/* One shared counter updated by N threads: mutex, spin lock (atomic_flag) or atomic fetch_add.
   cs_work > 0 adds work INSIDE the critical section (a long critical section). */
static int mode, nthreads;
static long total, cs_work, counter;
static atomic_long acounter, sink;
static pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
static atomic_flag spin = ATOMIC_FLAG_INIT;

static unsigned burn(long n, unsigned x)
{
    for (long i = 0; i < n; i++) x = x * 1664525u + 1013904223u;
    return x;
}

static void *worker(void *a)
{
    (void)a;
    long n = total / nthreads;
    unsigned acc = 0;
    for (long i = 0; i < n; i++) {
        if (mode == 0) {
            pthread_mutex_lock(&mx);
            counter++;
            if (cs_work) acc = burn(cs_work, acc);
            pthread_mutex_unlock(&mx);
        } else if (mode == 1) {
            while (atomic_flag_test_and_set_explicit(&spin, memory_order_acquire)) ;
            counter++;
            if (cs_work) acc = burn(cs_work, acc);
            atomic_flag_clear_explicit(&spin, memory_order_release);
        } else {
            atomic_fetch_add(&acounter, 1);
        }
    }
    atomic_fetch_add(&sink, acc);
    return NULL;
}

static double cpu_ms(void)
{
    struct rusage u;
    getrusage(RUSAGE_SELF, &u);
    return (u.ru_utime.tv_sec + u.ru_stime.tv_sec) * 1e3 + (u.ru_utime.tv_usec + u.ru_stime.tv_usec) / 1e3;
}

int main(int argc, char **argv)
{
    if (argc != 5) { fprintf(stderr, "usage: %s mutex|spin|atomic threads totalUpdates csWork\n", argv[0]); return 1; }
    mode = !strcmp(argv[1], "spin") ? 1 : !strcmp(argv[1], "atomic") ? 2 : 0;
    nthreads = atoi(argv[2]); total = atol(argv[3]); cs_work = atol(argv[4]);
    pthread_t th[64];
    struct timespec t0, t1;
    double c0 = cpu_ms();
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < nthreads; i++) pthread_create(&th[i], NULL, worker, NULL);
    for (int i = 0; i < nthreads; i++) pthread_join(th[i], NULL);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double wall = ((t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9) * 1e3, cpu = cpu_ms() - c0;
    long got = mode == 2 ? atomic_load(&acounter) : counter;
    long per = total / nthreads * nthreads;
    printf("mode=%s threads=%d updates=%ld cs_work=%ld wall=%.1f ms cpu=%.1f ms cpu_over_wall=%.2f ns_per_update=%.1f %s\n",
           argv[1], nthreads, per, cs_work, wall, cpu, cpu / wall, wall * 1e6 / per, got == per ? "OK" : "WRONG");
    return 0;
}
