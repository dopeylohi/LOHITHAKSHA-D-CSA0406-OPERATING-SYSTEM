#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>

/* Peterson's solution for two threads protecting a shared counter.
   A: plain volatile variables (the textbook code).  B: the same code with sequentially consistent atomics. */
static long ITER = 1000000;
static volatile int vflag[2], vturn;
static atomic_int aflag[2], aturn;
static volatile long counter;

static void lockA(int i)   { int j = 1 - i; vflag[i] = 1; vturn = j; while (vflag[j] && vturn == j) ; }
static void unlockA(int i) { vflag[i] = 0; }
static void lockB(int i)   { int j = 1 - i; atomic_store(&aflag[i], 1); atomic_store(&aturn, j);
                             while (atomic_load(&aflag[j]) && atomic_load(&aturn) == j) ; }
static void unlockB(int i) { atomic_store(&aflag[i], 0); }

static int variant;

static void *worker(void *a)
{
    int i = (int)(long)a;
    for (long k = 0; k < ITER; k++) {
        if (variant == 0) lockA(i); else lockB(i);
        counter = counter + 1;                       /* critical section */
        if (variant == 0) unlockA(i); else unlockB(i);
    }
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc >= 2) ITER = atol(argv[1]);
    const char *name[2] = {"A volatile (plain Peterson)", "B seq_cst atomics"};
    for (variant = 0; variant < 2; variant++) {
        counter = 0;
        pthread_t th[2];
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        for (long i = 0; i < 2; i++) pthread_create(&th[i], NULL, worker, (void *)i);
        for (int i = 0; i < 2; i++) pthread_join(th[i], NULL);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double ms = ((t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9) * 1e3;
        printf("%-28s expected=%ld got=%ld lost=%ld time=%.1f ms\n", name[variant], 2 * ITER, counter,
               2 * ITER - counter, ms);
    }
    return 0;
}
