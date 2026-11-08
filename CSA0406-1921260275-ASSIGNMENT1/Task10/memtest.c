#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#define N 1000

static void *w(void *a) { (void)a; pause(); return NULL; }

static long field(const char *path, const char *key)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[256]; long v = -1; size_t k = strlen(key);
    while (fgets(line, sizeof line, f))
        if (!strncmp(line, key, k) && line[k] == ':') { v = strtol(line + k + 1, NULL, 10); break; }
    fclose(f);
    return v;
}

int main(int argc, char **argv)
{
    if (argc != 2) { fprintf(stderr, "usage: %s threads|procs\n", argv[0]); return 1; }
    printf("baseline: VmSize %ld kB, VmRSS %ld kB, VmPTE %ld kB\n",
           field("/proc/self/status", "VmSize"), field("/proc/self/status", "VmRSS"), field("/proc/self/status", "VmPTE"));
    if (!strcmp(argv[1], "threads")) {
        pthread_t t;
        for (int i = 0; i < N; i++) {
            if (pthread_create(&t, NULL, w, NULL)) { perror("pthread_create"); return 1; }
            pthread_detach(t);
        }
        sleep(1);
        printf("%d threads alive: VmSize %ld kB, VmRSS %ld kB, VmPTE %ld kB\n", N,
               field("/proc/self/status", "VmSize"), field("/proc/self/status", "VmRSS"), field("/proc/self/status", "VmPTE"));
        return 0;
    }
    static pid_t pids[N];
    for (int i = 0; i < N; i++) {
        pids[i] = fork();
        if (pids[i] < 0) { perror("fork"); return 1; }
        if (pids[i] == 0) { pause(); _exit(0); }
    }
    sleep(1);
    long pss = 0, rss = 0, pte = 0, x; char p[64];
    for (int i = 0; i < N; i++) {
        snprintf(p, sizeof p, "/proc/%d/smaps_rollup", pids[i]); if ((x = field(p, "Pss")) > 0) pss += x;
        snprintf(p, sizeof p, "/proc/%d/status", pids[i]);
        if ((x = field(p, "VmRSS")) > 0) rss += x;
        if ((x = field(p, "VmPTE")) > 0) pte += x;
    }
    printf("%d processes alive: sum Pss %ld kB, sum VmRSS %ld kB (shared pages counted many times), sum VmPTE %ld kB\n", N, pss, rss, pte);
    for (int i = 0; i < N; i++) kill(pids[i], SIGKILL);
    for (int i = 0; i < N; i++) waitpid(pids[i], NULL, 0);
    return 0;
}
