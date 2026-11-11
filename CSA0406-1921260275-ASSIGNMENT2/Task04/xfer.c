#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>

/* Transfer N MiB from parent (writer) to child (reader) and verify a checksum.
   pipe : writer fills a private buffer, write() copies it into the kernel, read() copies it out.
   shm  : writer fills a shared slot directly, reader sums it in place (no copy), semaphores sync. */
#define CHUNK (64 * 1024)
#define WORDS (CHUNK / 8)
#define SLOTS 16

struct shared {
    double t_start, t_end;
    uint64_t checksum;
    sem_t empty, full;
    uint64_t slot[SLOTS][WORDS];
};

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

static uint64_t fill(uint64_t *p, uint64_t *ctr)
{
    for (int i = 0; i < WORDS; i++) p[i] = (*ctr)++;
    return 0;
}

static uint64_t sum(const uint64_t *p)
{
    uint64_t s = 0;
    for (int i = 0; i < WORDS; i++) s += p[i];
    return s;
}

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "usage: %s pipe|shm MiB\n", argv[0]); return 1; }
    int use_pipe = !strcmp(argv[1], "pipe");
    long mib = atol(argv[2]);
    long chunks = mib * 1024 * 1024 / CHUNK;
    struct shared *s = mmap(NULL, sizeof *s, PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (s == MAP_FAILED) { perror("mmap"); return 1; }
    sem_init(&s->empty, 1, SLOTS);
    sem_init(&s->full, 1, 0);
    int fd[2];
    if (use_pipe && pipe(fd) != 0) { perror("pipe"); return 1; }

    pid_t pid = fork();
    if (pid == 0) {                                      /* reader */
        uint64_t total = 0;
        if (use_pipe) {
            close(fd[1]);
            uint64_t *buf = malloc(CHUNK);
            for (long c = 0; c < chunks; c++) {
                size_t got = 0;
                while (got < CHUNK) {
                    ssize_t r = read(fd[0], (char *)buf + got, CHUNK - got);
                    if (r <= 0) _exit(2);
                    got += (size_t)r;
                }
                total += sum(buf);
            }
        } else {
            int out = 0;
            for (long c = 0; c < chunks; c++) {
                sem_wait(&s->full);
                total += sum(s->slot[out]);              /* read in place */
                out = (out + 1) % SLOTS;
                sem_post(&s->empty);
            }
        }
        s->t_end = now();
        s->checksum = total;
        _exit(0);
    }
    uint64_t ctr = 0;
    s->t_start = now();
    if (use_pipe) {
        close(fd[0]);
        uint64_t *buf = malloc(CHUNK);
        for (long c = 0; c < chunks; c++) {
            fill(buf, &ctr);
            size_t put = 0;
            while (put < CHUNK) {
                ssize_t w = write(fd[1], (char *)buf + put, CHUNK - put);
                if (w <= 0) { perror("write"); return 1; }
                put += (size_t)w;
            }
        }
        close(fd[1]);
    } else {
        int in = 0;
        for (long c = 0; c < chunks; c++) {
            sem_wait(&s->empty);
            fill(s->slot[in], &ctr);                     /* write straight into shared memory */
            in = (in + 1) % SLOTS;
            sem_post(&s->full);
        }
    }
    waitpid(pid, NULL, 0);
    uint64_t n = ctr, expect = n * (n - 1) / 2;
    double ms = (s->t_end - s->t_start) * 1e3;
    printf("%-4s %4ld MiB  time=%9.3f ms  rate=%8.1f MiB/s  checksum %s\n", argv[1], mib, ms,
           mib / (ms / 1e3), s->checksum == expect ? "OK" : "WRONG");
    return 0;
}
