#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>

/* Shared memory WITHOUT any synchronisation: one shared slot, no semaphore.
   The writer publishes N readings 1..N; the reader counts how many different values it sees. */
#define N 1000000
struct shared { volatile int value; volatile int done; };

int main(void)
{
    struct shared *s = mmap(NULL, sizeof *s, PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (s == MAP_FAILED) { perror("mmap"); return 1; }
    s->value = 0; s->done = 0;
    pid_t pid = fork();
    if (pid == 0) {
        int last = 0;
        long seen = 0;
        while (!s->done) {
            int v = s->value;
            if (v != last) { seen++; last = v; }
        }
        if (s->value != last) seen++;
        printf("no sync: writer sent %d readings, reader saw %ld different ones, lost %ld\n",
               N, seen, N - seen);
        fflush(stdout);
        _exit(0);
    }
    for (int i = 1; i <= N; i++) s->value = i;
    s->done = 1;
    waitpid(pid, NULL, 0);
    return 0;
}
