#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>

static int global_var = 100;       /* data segment : shared by all threads */
static int *heap_block;            /* heap         : shared by all threads */
static int shared_fd;              /* file table   : shared by all threads */

static void *worker(void *arg)
{
    int id = *(int *)arg;
    int local = id * 10;           /* stack        : private to each thread */
    errno = id;                    /* errno        : private to each thread */
    usleep(300000 + id * 100000);  /* stagger so the lines print in order */
    printf("thread %d: pid=%d tid=%ld | &global=%p heap=%p fd_valid=%s | &local=%p errno=%d\n",
           id, (int)getpid(), (long)syscall(SYS_gettid), (void *)&global_var, (void *)heap_block,
           fcntl(shared_fd, F_GETFD) >= 0 ? "yes" : "no", (void *)&local, errno);
    return NULL;
}

int main(void)
{
    heap_block = malloc(sizeof *heap_block);
    shared_fd = open("/dev/null", O_RDONLY);
    pthread_t th[3];
    int id[3] = {1, 2, 3};
    printf("main:     pid=%d tid=%ld | &global=%p heap=%p\n", (int)getpid(),
           (long)syscall(SYS_gettid), (void *)&global_var, (void *)heap_block);
    for (int i = 0; i < 3; i++) pthread_create(&th[i], NULL, worker, &id[i]);
    usleep(150000);
    char cmd[64];
    snprintf(cmd, sizeof cmd, "echo '/proc/%d/task contains:'; ls /proc/%d/task", (int)getpid(), (int)getpid());
    if (system(cmd) == -1) perror("system");
    for (int i = 0; i < 3; i++) pthread_join(th[i], NULL);
    return 0;
}
