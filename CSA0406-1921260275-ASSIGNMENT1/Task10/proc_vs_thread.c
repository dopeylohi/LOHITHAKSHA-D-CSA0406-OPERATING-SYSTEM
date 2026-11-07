#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <spawn.h>
#include <time.h>
#include <sys/wait.h>

extern char **environ;
#define N 1000

static double now(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

static void *worker(void *a) { (void)a; return NULL; }

int main(void)
{
    double t0, t1;
    pid_t p;
    pthread_t th;
    char *args[] = {"true", NULL};

    t0 = now();
    for (int i = 0; i < N; i++) {
        p = fork();
        if (p == 0) _exit(0);
        waitpid(p, NULL, 0);
    }
    t1 = now();
    printf("1000 x fork() only        : %.4f s\n", t1 - t0);

    t0 = now();
    for (int i = 0; i < N; i++) {
        pthread_create(&th, NULL, worker, NULL);
        pthread_join(th, NULL);
    }
    t1 = now();
    printf("1000 x pthread_create     : %.4f s\n", t1 - t0);

    t0 = now();
    for (int i = 0; i < N; i++) {
        p = fork();
        if (p == 0) { execl("/bin/true", "true", (char *)NULL); _exit(127); }
        waitpid(p, NULL, 0);
    }
    t1 = now();
    printf("1000 x fork() + exec()    : %.4f s\n", t1 - t0);

    t0 = now();
    for (int i = 0; i < N; i++) {
        if (posix_spawn(&p, "/bin/true", NULL, NULL, args, environ) != 0) return 1;
        waitpid(p, NULL, 0);
    }
    t1 = now();
    printf("1000 x posix_spawn()      : %.4f s\n", t1 - t0);
    return 0;
}
