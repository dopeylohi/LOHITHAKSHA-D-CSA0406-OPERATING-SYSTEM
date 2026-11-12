#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>

/* Classic problems with POSIX semaphores.
   phil naive   : 5 philosophers, each takes the left fork then the right fork        (can deadlock)
   phil ordered : the last philosopher takes the right fork first (breaks the circle) (deadlock-free)
   phil room    : a semaphore lets at most 4 philosophers sit down                    (deadlock-free)
   rw sem       : readers-writers with semaphores (mutex + wrt)
   rw nosync    : the same threads with no synchronisation at all */
#define NPH 5
static sem_t fork_[NPH], room;
static atomic_int meals[NPH], stop_flag;
static int pmode;                              /* 0 naive, 1 ordered, 2 room */

static void *phil(void *arg)
{
    int i = (int)(long)arg, l = i, r = (i + 1) % NPH;
    unsigned s = (unsigned)(i + 1) * 2654435761u;
    while (!atomic_load(&stop_flag)) {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        usleep(s % 500);                                        /* think */
        if (pmode == 2) sem_wait(&room);
        int first = l, second = r;
        if (pmode == 1 && i == NPH - 1) { first = r; second = l; }
        sem_wait(&fork_[first]);
        usleep(1000);                                           /* delay that makes the circular wait likely */
        sem_wait(&fork_[second]);
        atomic_fetch_add(&meals[i], 1);                         /* eat */
        usleep(200);
        sem_post(&fork_[second]);
        sem_post(&fork_[first]);
        if (pmode == 2) sem_post(&room);
    }
    return NULL;
}

static long total_meals(void)
{
    long t = 0;
    for (int i = 0; i < NPH; i++) t += atomic_load(&meals[i]);
    return t;
}

static int run_phil(const char *which)
{
    pmode = !strcmp(which, "ordered") ? 1 : !strcmp(which, "room") ? 2 : 0;
    for (int i = 0; i < NPH; i++) sem_init(&fork_[i], 0, 1);
    sem_init(&room, 0, NPH - 1);
    pthread_t th[NPH];
    for (long i = 0; i < NPH; i++) pthread_create(&th[i], NULL, phil, (void *)i);
    long last = -1;
    int stalled = 0;
    for (int tick = 0; tick < 6; tick++) {                      /* observe for 3 s, every 0.5 s */
        usleep(500000);
        long now = total_meals();
        stalled = (now == last) ? stalled + 1 : 0;
        last = now;
        if (stalled >= 2) {
            printf("phil %-7s: DEADLOCK - nobody has eaten for 1 s (total meals %ld); meals: %d %d %d %d %d\n",
                   which, now, meals[0], meals[1], meals[2], meals[3], meals[4]);
            fflush(stdout);
            _exit(0);
        }
    }
    atomic_store(&stop_flag, 1);
    for (int i = 0; i < NPH; i++) pthread_join(th[i], NULL);
    printf("phil %-7s: no deadlock in 3 s, total meals %ld; meals: %d %d %d %d %d\n", which, total_meals(),
           meals[0], meals[1], meals[2], meals[3], meals[4]);
    return 0;
}

/* ---------------- readers-writers ---------------- */
static sem_t rmutex, wrt;
static int read_count, A, B, use_sync;
static atomic_long reads, writes, bad;

static void *reader(void *arg)
{
    (void)arg;
    while (!atomic_load(&stop_flag)) {
        if (use_sync) {
            sem_wait(&rmutex);
            if (++read_count == 1) sem_wait(&wrt);             /* first reader locks writers out */
            sem_post(&rmutex);
        }
        int a = *(volatile int *)&A;
        usleep(20);
        int b = *(volatile int *)&B;
        if (a != b) atomic_fetch_add(&bad, 1);                  /* saw a half-finished update */
        atomic_fetch_add(&reads, 1);
        if (use_sync) {
            sem_wait(&rmutex);
            if (--read_count == 0) sem_post(&wrt);             /* last reader lets writers in */
            sem_post(&rmutex);
        }
        usleep(10);
    }
    return NULL;
}

static void *writer(void *arg)
{
    (void)arg;
    while (!atomic_load(&stop_flag)) {
        if (use_sync) sem_wait(&wrt);
        *(volatile int *)&A += 1;
        usleep(50);                                             /* update takes time: A and B differ meanwhile */
        *(volatile int *)&B += 1;
        atomic_fetch_add(&writes, 1);
        if (use_sync) sem_post(&wrt);
        usleep(200);
    }
    return NULL;
}

static int run_rw(const char *which)
{
    use_sync = !strcmp(which, "sem");
    sem_init(&rmutex, 0, 1);
    sem_init(&wrt, 0, 1);
    pthread_t r[4], w[2];
    for (int i = 0; i < 4; i++) pthread_create(&r[i], NULL, reader, NULL);
    for (int i = 0; i < 2; i++) pthread_create(&w[i], NULL, writer, NULL);
    usleep(2000000);
    atomic_store(&stop_flag, 1);
    for (int i = 0; i < 4; i++) pthread_join(r[i], NULL);
    for (int i = 0; i < 2; i++) pthread_join(w[i], NULL);
    printf("rw %-7s: reads=%ld writes=%ld inconsistent reads=%ld\n", which, (long)reads, (long)writes, (long)bad);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 3 && !strcmp(argv[1], "phil")) return run_phil(argv[2]);
    if (argc == 3 && !strcmp(argv[1], "rw")) return run_rw(argv[2]);
    fprintf(stderr, "usage: %s phil naive|ordered|room | rw sem|nosync\n", argv[0]);
    return 1;
}
