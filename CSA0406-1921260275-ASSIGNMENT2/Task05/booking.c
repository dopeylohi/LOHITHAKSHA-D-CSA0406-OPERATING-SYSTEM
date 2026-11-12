#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

/* Railway booking: 64 trains x 16 coaches x 256 seats.  Each attempt picks a random coach, finds the
   first free seat, does some "fare / payment check" work, then marks the seat sold.
   Lock granularity: none | global (1 lock) | train (64 locks) | coach (1024 locks). */
#define TRAINS   64
#define COACHES  16
#define SEATS    256
#define ATTEMPTS 200000
#define WORK_IN  800          /* simulated work inside the critical section  */
#define WORK_OUT 200          /* simulated customer work outside it          */

typedef struct { pthread_mutex_t m; char pad[64]; } padlock;     /* one lock per cache line */
static padlock glock, tlock[TRAINS], clock_[TRAINS * COACHES];
static unsigned char seat[TRAINS][COACHES][SEATS];
enum { NONE, GLOBAL, TRAIN, COACH };
static int mode, nthreads;
static struct { long ok; unsigned acc; char pad[48]; } res[64];

static unsigned burn(unsigned n, unsigned x)
{
    for (unsigned i = 0; i < n; i++) x = x * 1664525u + 1013904223u;
    return x;
}

static void *agent(void *arg)
{
    long id = (long)arg, ok = 0;
    unsigned s = (unsigned)(id + 1) * 2654435761u, acc = 0;
    for (long a = 0; a < ATTEMPTS / nthreads; a++) {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;                  /* xorshift random numbers */
        int t = (int)(s % TRAINS), c = (int)((s >> 8) % COACHES);
        acc = burn(WORK_OUT, acc + s);
        pthread_mutex_t *m = NULL;
        if (mode == GLOBAL) m = &glock.m;
        else if (mode == TRAIN) m = &tlock[t].m;
        else if (mode == COACH) m = &clock_[t * COACHES + c].m;
        if (m) pthread_mutex_lock(m);
        int i = 0;
        while (i < SEATS && seat[t][c][i]) i++;                    /* first free seat */
        if (i < SEATS) {
            acc = burn(WORK_IN, acc);
            seat[t][c][i] = 1;                                     /* sell it */
            ok++;
        }
        if (m) pthread_mutex_unlock(m);
    }
    res[id].ok = ok; res[id].acc = acc;
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "usage: %s none|global|train|coach threads\n", argv[0]); return 1; }
    mode = !strcmp(argv[1], "global") ? GLOBAL : !strcmp(argv[1], "train") ? TRAIN :
           !strcmp(argv[1], "coach") ? COACH : NONE;
    nthreads = atoi(argv[2]);
    pthread_mutex_init(&glock.m, NULL);
    for (int i = 0; i < TRAINS; i++) pthread_mutex_init(&tlock[i].m, NULL);
    for (int i = 0; i < TRAINS * COACHES; i++) pthread_mutex_init(&clock_[i].m, NULL);
    memset(seat, 0, sizeof seat);
    pthread_t th[64];
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (long i = 0; i < nthreads; i++) pthread_create(&th[i], NULL, agent, (void *)i);
    for (int i = 0; i < nthreads; i++) pthread_join(th[i], NULL);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double sec = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    long bookings = 0, occupied = 0;
    for (int i = 0; i < nthreads; i++) bookings += res[i].ok;
    for (int t = 0; t < TRAINS; t++)
        for (int c = 0; c < COACHES; c++)
            for (int s = 0; s < SEATS; s++) occupied += seat[t][c][s];
    printf("mode=%s threads=%d time=%.1f ms bookings=%ld occupied=%ld oversold=%ld throughput=%.0f\n",
           argv[1], nthreads, sec * 1e3, bookings, occupied, bookings - occupied, bookings / sec);
    return 0;
}
