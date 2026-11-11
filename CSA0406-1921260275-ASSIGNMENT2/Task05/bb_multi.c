#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>

/* Bounded buffer (SIZE 5) with P producers and C consumers.
   impl: sem      = empty/full semaphores + mutex   (the given design)
         nomutex  = semaphores only, mutex removed
         monitor  = one mutex + two condition variables
   Producer p makes the unique ids p*IPP+1 .. p*IPP+IPP, so "consumed exactly once" can be checked. */
#define SIZE 5
#define MAXITEMS 400000

static int buffer[SIZE], in, out, count;
static sem_t empty, full;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t not_full = PTHREAD_COND_INITIALIZER, not_empty = PTHREAD_COND_INITIALIZER;
static atomic_int seen[MAXITEMS + 1];
static atomic_long sum_total;
static int impl, P, C, IPP;                      /* impl: 0 sem, 1 nomutex, 2 monitor */

static void put(int item)
{
    if (impl == 2) {
        pthread_mutex_lock(&lock);
        while (count == SIZE) pthread_cond_wait(&not_full, &lock);
        buffer[in] = item; in = (in + 1) % SIZE; count++;
        pthread_cond_signal(&not_empty);
        pthread_mutex_unlock(&lock);
        return;
    }
    sem_wait(&empty);
    if (impl == 0) pthread_mutex_lock(&lock);
    buffer[in] = item;
    in = (in + 1) % SIZE;
    if (impl == 0) pthread_mutex_unlock(&lock);
    sem_post(&full);
}

static int get(void)
{
    int item;
    if (impl == 2) {
        pthread_mutex_lock(&lock);
        while (count == 0) pthread_cond_wait(&not_empty, &lock);
        item = buffer[out]; out = (out + 1) % SIZE; count--;
        pthread_cond_signal(&not_full);
        pthread_mutex_unlock(&lock);
        return item;
    }
    sem_wait(&full);
    if (impl == 0) pthread_mutex_lock(&lock);
    item = buffer[out];
    out = (out + 1) % SIZE;
    if (impl == 0) pthread_mutex_unlock(&lock);
    sem_post(&empty);
    return item;
}

static void *producer(void *a)
{
    long p = (long)a;
    for (int k = 1; k <= IPP; k++) put((int)(p * IPP + k));
    return NULL;
}

static void *consumer(void *a)
{
    (void)a;
    long local = 0;
    int mine = P * IPP / C;
    for (int k = 0; k < mine; k++) {
        int item = get();
        if (item >= 1 && item <= MAXITEMS) atomic_fetch_add(&seen[item], 1);
        local += item;
    }
    atomic_fetch_add(&sum_total, local);
    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 5) { fprintf(stderr, "usage: %s sem|nomutex|monitor P C itemsPerProducer\n", argv[0]); return 1; }
    impl = !strcmp(argv[1], "sem") ? 0 : !strcmp(argv[1], "nomutex") ? 1 : 2;
    P = atoi(argv[2]); C = atoi(argv[3]); IPP = atoi(argv[4]);
    int total = P * IPP;
    if (total > MAXITEMS || total % C != 0) { fprintf(stderr, "bad parameters\n"); return 1; }
    sem_init(&empty, 0, SIZE);
    sem_init(&full, 0, 0);
    pthread_t pt[64], ct[64];
    for (long i = 0; i < P; i++) pthread_create(&pt[i], NULL, producer, (void *)i);
    for (long i = 0; i < C; i++) pthread_create(&ct[i], NULL, consumer, NULL);
    for (int i = 0; i < P; i++) pthread_join(pt[i], NULL);
    for (int i = 0; i < C; i++) pthread_join(ct[i], NULL);
    int missing = 0, dup = 0;
    for (int id = 1; id <= total; id++) {
        int n = atomic_load(&seen[id]);
        if (n == 0) missing++;
        else if (n > 1) dup++;
    }
    long expect = (long)total * (total + 1) / 2;
    int ok = missing == 0 && dup == 0 && atomic_load(&sum_total) == expect;
    printf("impl=%s P=%d C=%d items=%d sum=%ld expected=%ld missing=%d duplicated=%d %s\n", argv[1], P, C,
           total, atomic_load(&sum_total), expect, missing, dup, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
