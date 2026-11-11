#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <pthread.h>
#include <sys/wait.h>

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

static long mem_avail_kb(void)
{
    FILE *f = fopen("/proc/meminfo", "r");
    char line[128];
    long v = -1;
    if (!f) return -1;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "MemAvailable: %ld kB", &v) == 1) break;
    fclose(f);
    return v;
}

static void pin_cpu0(void)
{
    cpu_set_t s;
    CPU_ZERO(&s);
    CPU_SET(0, &s);
    if (sched_setaffinity(0, sizeof s, &s) != 0) perror("sched_setaffinity");
}

/* ---------- spawn: N sleeping tasks, as 'student sessions' ---------- */
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int release_all = 0, started = 0;

static void *sleeper(void *a)
{
    (void)a;
    pthread_mutex_lock(&m);
    started++;
    while (!release_all) pthread_cond_wait(&cv, &m);
    pthread_mutex_unlock(&m);
    return NULL;
}

static int spawn(int use_threads, int n)
{
    pid_t *pids = malloc(n * sizeof *pids);
    pthread_t *th = malloc(n * sizeof *th);
    long before = mem_avail_kb();
    double t0 = now();

    if (use_threads) {
        pthread_attr_t at;
        pthread_attr_init(&at);
        pthread_attr_setstacksize(&at, 64 * 1024);      /* 64 KB stack per thread */
        for (int i = 0; i < n; i++)
            if (pthread_create(&th[i], &at, sleeper, NULL) != 0) {
                fprintf(stderr, "pthread_create failed at %d\n", i); return 1;
            }
    } else {
        for (int i = 0; i < n; i++) {
            pids[i] = fork();
            if (pids[i] < 0) { perror("fork"); return 1; }
            if (pids[i] == 0) { pause(); _exit(0); }
        }
    }
    double t1 = now();
    if (use_threads) {                                  /* wait until all threads are blocked */
        for (;;) {
            pthread_mutex_lock(&m);
            int s = started;
            pthread_mutex_unlock(&m);
            if (s == n) break;
            usleep(1000);
        }
    }
    usleep(500000);                                     /* let memory settle */
    long after = mem_avail_kb();

    printf("%-7s n=%d  create_total=%8.1f ms  per_task=%7.1f us  "
           "MemAvailable_drop=%8ld kB  per_task=%6.1f kB\n",
           use_threads ? "threads" : "procs", n, (t1 - t0) * 1e3,
           (t1 - t0) * 1e6 / n, before - after, (double)(before - after) / n);

    if (use_threads) {
        pthread_mutex_lock(&m);
        release_all = 1;
        pthread_cond_broadcast(&cv);
        pthread_mutex_unlock(&m);
        for (int i = 0; i < n; i++) pthread_join(th[i], NULL);
    } else {
        for (int i = 0; i < n; i++) kill(pids[i], SIGTERM);
        for (int i = 0; i < n; i++) waitpid(pids[i], NULL, 0);
    }
    free(pids); free(th);
    return 0;
}

/* ---------- pingpong: two tasks on ONE CPU hand a byte back and forth ---------- */
static int p2c[2], c2p[2];
static long rounds;

static void *echo_thread(void *a)
{
    (void)a;
    char b;
    for (long i = 0; i < rounds; i++) {
        if (read(p2c[0], &b, 1) != 1) break;
        if (write(c2p[1], &b, 1) != 1) break;
    }
    return NULL;
}

static int pingpong(int use_threads, long r)
{
    rounds = r;
    if (pipe(p2c) || pipe(c2p)) { perror("pipe"); return 1; }
    pin_cpu0();                                         /* children/threads inherit this */
    pid_t pid = 0;
    pthread_t th;
    if (use_threads) {
        pthread_create(&th, NULL, echo_thread, NULL);
    } else {
        pid = fork();
        if (pid == 0) { echo_thread(NULL); _exit(0); }
    }
    char b = 'x';
    double t0 = now();
    for (long i = 0; i < r; i++) {
        if (write(p2c[1], &b, 1) != 1) break;
        if (read(c2p[0], &b, 1) != 1) break;
    }
    double t1 = now();
    if (use_threads) pthread_join(th, NULL); else waitpid(pid, NULL, 0);
    double rt = (t1 - t0) * 1e6 / r;
    printf("%-7s pingpong rounds=%ld  per_round_trip=%6.2f us  per_switch(approx)=%6.2f us\n",
           use_threads ? "threads" : "procs", r, rt, rt / 2);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 4) {
        fprintf(stderr, "usage: %s spawn|pingpong procs|threads N\n", argv[0]);
        return 1;
    }
    int th = !strcmp(argv[2], "threads");
    long n = atol(argv[3]);
    if (!strcmp(argv[1], "spawn")) return spawn(th, (int)n);
    if (!strcmp(argv[1], "pingpong")) return pingpong(th, n);
    fprintf(stderr, "unknown mode\n");
    return 1;
}
