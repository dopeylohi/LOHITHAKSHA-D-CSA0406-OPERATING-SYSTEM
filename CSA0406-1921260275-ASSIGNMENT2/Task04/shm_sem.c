#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>

/* Same exchange as pipe_ok.c, but through shared memory + two counting semaphores.
   A small ring of 4 slots makes the producer wait when the analyser is behind. */
#define SLOTS 4
#define END_MARK (-1)                       /* sentinel: no more readings */

struct shared {
    int slot[SLOTS];
    sem_t empty;                            /* counts free slots   */
    sem_t full;                             /* counts filled slots */
};

int main(void)
{
    struct shared *s = mmap(NULL, sizeof *s, PROT_READ | PROT_WRITE,
                            MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (s == MAP_FAILED) { perror("mmap"); return 1; }
    sem_init(&s->empty, 1, SLOTS);          /* pshared = 1: shared between processes */
    sem_init(&s->full, 1, 0);

    pid_t pid = fork();
    if (pid == 0) {                         /* analyser (consumer) */
        int out = 0, count = 0, sum = 0, max = 0;
        for (;;) {
            sem_wait(&s->full);
            int v = s->slot[out];
            out = (out + 1) % SLOTS;
            sem_post(&s->empty);
            if (v == END_MARK) break;
            count++; sum += v;
            if (v > max) max = v;
        }
        printf("analyser: %d readings, average %.1f, max %d\n", count,
               count ? (double)sum / count : 0.0, max);
        fflush(stdout);
        _exit(0);
    }
    int readings[] = {31, 33, 36, 34, 41, 38};   /* sensor reader (producer) */
    int in = 0;
    for (int i = 0; i < 7; i++) {
        int v = i < 6 ? readings[i] : END_MARK;
        sem_wait(&s->empty);
        s->slot[in] = v;
        in = (in + 1) % SLOTS;
        sem_post(&s->full);
    }
    waitpid(pid, NULL, 0);
    printf("reader: done\n");
    sem_destroy(&s->empty);
    sem_destroy(&s->full);
    return 0;
}
