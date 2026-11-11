#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <sys/wait.h>

#define CAMS   64
#define FRAMES 80
#define W      320
#define H      240

/* ---------- the "number-plate reader": CPU-bound work on a synthetic frame ---------- */
static unsigned long process_frame(unsigned char *img, unsigned char *out, unsigned cam, unsigned frame)
{
    unsigned s = cam * 7919u + frame * 104729u + 12345u;
    for (int i = 0; i < W * H; i++) { s = s * 1664525u + 1013904223u; img[i] = (unsigned char)(s >> 24); }
    for (int y = 1; y < H - 1; y++)                      /* 3x3 blur */
        for (int x = 1; x < W - 1; x++) {
            int sum = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) sum += img[(y + dy) * W + x + dx];
            out[y * W + x] = (unsigned char)(sum / 9);
        }
    unsigned long count = 0;                             /* "plate-like" pixels = strong local contrast */
    for (int y = 1; y < H - 1; y++)
        for (int x = 1; x < W - 1; x++) {
            int d = img[y * W + x] - out[y * W + x];
            if (d < 0) d = -d;
            if (d > 40) count++;
        }
    return count;
}

static unsigned long *result;                            /* result[cam]: one private slot per camera */

static void camera(int c, unsigned char *img, unsigned char *out)
{
    unsigned long total = 0;
    for (int f = 0; f < FRAMES; f++) total += process_frame(img, out, (unsigned)c, (unsigned)f);
    result[c] = total;
}

static unsigned long checksum(void)
{
    unsigned long s = 0;
    for (int c = 0; c < CAMS; c++) s += result[c] * (unsigned long)(c + 1);
    return s;
}

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

/* ---------- threads: a pool of N workers takes the next unprocessed camera ---------- */
static atomic_int next_cam;

static void *worker(void *a)
{
    (void)a;
    unsigned char *img = malloc(W * H), *out = malloc(W * H);   /* private buffers */
    int c;
    while ((c = atomic_fetch_add(&next_cam, 1)) < CAMS) camera(c, img, out);
    free(img); free(out);
    return NULL;
}

static void run_threads(int n)
{
    pthread_t th[CAMS];
    atomic_store(&next_cam, 0);
    for (int i = 0; i < n; i++) pthread_create(&th[i], NULL, worker, NULL);
    for (int i = 0; i < n; i++) pthread_join(th[i], NULL);
}

/* ---------- processes: camera k, k+n, k+2n ... go to child k ---------- */
struct rec { int cam; unsigned long total; };

static void run_procs(int n, int use_pipe)
{
    int fd[CAMS][2];
    pid_t pid[CAMS];
    for (int k = 0; k < n; k++) {
        if (use_pipe && pipe(fd[k]) != 0) { perror("pipe"); exit(1); }
        pid[k] = fork();
        if (pid[k] < 0) { perror("fork"); exit(1); }
        if (pid[k] == 0) {
            unsigned char *img = malloc(W * H), *out = malloc(W * H);
            for (int c = k; c < CAMS; c += n) {
                camera(c, img, out);                     /* writes result[c] (shared mmap or private copy) */
                if (use_pipe) {
                    struct rec r = { c, result[c] };
                    if (write(fd[k][1], &r, sizeof r) != (ssize_t)sizeof r) _exit(2);
                }
            }
            _exit(0);
        }
        if (use_pipe) close(fd[k][1]);
    }
    if (use_pipe) {
        for (int k = 0; k < n; k++) {
            struct rec r;
            while (read(fd[k][0], &r, sizeof r) == (ssize_t)sizeof r) result[r.cam] = r.total;
            close(fd[k][0]);
        }
    }
    for (int k = 0; k < n; k++) waitpid(pid[k], NULL, 0);
}

/* ---------- race demo: 8 threads x 1,000,000 increments of one shared counter ---------- */
#define RT 8
#define RN 1000000
static volatile long c_unsafe;
static long c_mutex;
static atomic_long c_atomic;
static pthread_mutex_t mx = PTHREAD_MUTEX_INITIALIZER;
static int race_mode;

static void *race_worker(void *a)
{
    (void)a;
    for (long i = 0; i < RN; i++) {
        if (race_mode == 0) c_unsafe++;                                          /* no protection */
        else if (race_mode == 1) { pthread_mutex_lock(&mx); c_mutex++; pthread_mutex_unlock(&mx); }
        else atomic_fetch_add(&c_atomic, 1);
    }
    return NULL;
}

static void run_race(void)
{
    const char *label[3] = {"unprotected", "mutex", "atomic"};
    for (race_mode = 0; race_mode < 3; race_mode++) {
        pthread_t th[RT];
        double t0 = now();
        for (int i = 0; i < RT; i++) pthread_create(&th[i], NULL, race_worker, NULL);
        for (int i = 0; i < RT; i++) pthread_join(th[i], NULL);
        double ms = (now() - t0) * 1e3;
        long got = race_mode == 0 ? c_unsafe : race_mode == 1 ? c_mutex : atomic_load(&c_atomic);
        printf("%-12s expected=%d got=%ld lost=%ld time=%.1f ms\n", label[race_mode], RT * RN, got,
               (long)RT * RN - got, ms);
    }
}

int main(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "race")) { run_race(); return 0; }
    int shared = argc >= 4 && !strcmp(argv[3], "shm");
    result = mmap(NULL, CAMS * sizeof *result, PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (result == MAP_FAILED) { perror("mmap"); return 1; }
    memset(result, 0, CAMS * sizeof *result);
    char mode[48];
    double t0 = now();
    if (argc >= 2 && !strcmp(argv[1], "seq")) {
        unsigned char *img = malloc(W * H), *out = malloc(W * H);
        for (int c = 0; c < CAMS; c++) camera(c, img, out);
        snprintf(mode, sizeof mode, "seq threads=1");
    } else if (argc >= 3 && !strcmp(argv[1], "pool")) {
        int n = atoi(argv[2]);
        run_threads(n);
        snprintf(mode, sizeof mode, "pool threads=%d", n);
    } else if (argc >= 4 && !strcmp(argv[1], "procs")) {
        int n = atoi(argv[2]);
        run_procs(n, !shared);
        snprintf(mode, sizeof mode, "procs-%s n=%d", shared ? "shm" : "pipe", n);
    } else {
        fprintf(stderr, "usage: %s seq | pool N | procs N shm|pipe | race\n", argv[0]);
        return 1;
    }
    double ms = (now() - t0) * 1e3;
    printf("mode=%s wall=%.1f ms checksum=%lu\n", mode, ms, checksum());
    return 0;
}
