#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/resource.h>
static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000.0 + t.tv_nsec / 1e6; }
static void stats(const char *who)
{
    FILE *f = fopen("/proc/self/smaps_rollup", "r");
    char line[256];
    long rss = -1, pss = -1, pd = -1, sd = -1;
    if (f) {
        while (fgets(line, sizeof line, f)) {
            sscanf(line, "Rss: %ld", &rss); sscanf(line, "Pss: %ld", &pss);
            sscanf(line, "Private_Dirty: %ld", &pd); sscanf(line, "Shared_Dirty: %ld", &sd);
        }
        fclose(f);
    }
    printf("%-28s Rss %7ld kB  Pss %7ld kB  Private_Dirty %7ld kB  Shared_Dirty %7ld kB\n", who, rss, pss, pd, sd);
    fflush(stdout);
}
static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "cow";
    long ps = sysconf(_SC_PAGESIZE);
    if (!strcmp(mode, "time")) {
        int sizes[4] = {1, 16, 64, 256};
        printf("fork() time in the parent, 5 runs each, median [ms]\n");
        for (int s = 0; s < 4; s++) {
            size_t bytes = (size_t)sizes[s] << 20;
            volatile char *p = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (p == MAP_FAILED) { perror("mmap"); return 1; }
            for (size_t i = 0; i < bytes; i += (size_t)ps) p[i] = 1;
            double t[5];
            for (int r = 0; r < 5; r++) {
                double t0 = now_ms();
                pid_t pid = fork();
                if (pid == 0) _exit(0);
                t[r] = now_ms() - t0;
                waitpid(pid, NULL, 0);
            }
            double u[5]; memcpy(u, t, sizeof t); qsort(u, 5, sizeof(double), cmpd);
            printf("%4d MB: runs %.3f %.3f %.3f %.3f %.3f  median %.3f ms\n", sizes[s], t[0], t[1], t[2], t[3], t[4], u[2]);
            munmap((void *)p, bytes);
        }
        return 0;
    }
    size_t mb = argc > 2 ? (size_t)atol(argv[2]) : 64;
    size_t bytes = mb << 20;
    volatile char *p = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) { perror("mmap"); return 1; }
    for (size_t i = 0; i < bytes; i += (size_t)ps) p[i] = 1;
    printf("block of %zu MB = %zu pages written by the parent\n", mb, bytes / (size_t)ps);
    stats("parent before fork");
    int c2p[2], p2c[2];
    if (pipe(c2p) || pipe(p2c)) { perror("pipe"); return 1; }
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); return 1; }
    if (pid == 0) {
        char b = 'x';
        struct rusage a, z;
        stats("child after fork, no write");
        getrusage(RUSAGE_SELF, &a);
        for (size_t i = 0; i < bytes; i += (size_t)ps) p[i] = 2;
        getrusage(RUSAGE_SELF, &z);
        printf("child wrote 1 byte in every page: minor faults %ld (copy-on-write faults)\n", z.ru_minflt - a.ru_minflt);
        stats("child after writing");
        if (write(c2p[1], &b, 1) != 1) _exit(1);
        if (read(p2c[0], &b, 1) != 1) _exit(1);
        _exit(0);
    }
    char b;
    if (read(c2p[0], &b, 1) != 1) return 1;
    stats("parent, child has written");
    if (write(p2c[1], &b, 1) != 1) return 1;
    waitpid(pid, NULL, 0);
    return 0;
}
