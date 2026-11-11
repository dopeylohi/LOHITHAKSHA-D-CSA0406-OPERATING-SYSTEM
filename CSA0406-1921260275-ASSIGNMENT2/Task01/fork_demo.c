#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>

#define NCHILD 3

/* Print the PCB-like fields the kernel keeps for a process (from /proc/<pid>/status) */
static void show_proc(pid_t pid, const char *role)
{
    char path[64], line[256];
    snprintf(path, sizeof path, "/proc/%d/status", (int)pid);
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return; }
    printf("--- %s ---\n", role);
    while (fgets(line, sizeof line, f))
        if (!strncmp(line, "Name:", 5) || !strncmp(line, "State:", 6) ||
            !strncmp(line, "Pid:", 4) || !strncmp(line, "PPid:", 5) ||
            !strncmp(line, "VmRSS:", 6) ||
            !strncmp(line, "voluntary_ctxt_switches:", 24) ||
            !strncmp(line, "nonvoluntary_ctxt_switches:", 27))
            fputs(line, stdout);
    fclose(f);
}

int main(void)
{
    const char *role[NCHILD] = {
        "child 0: CPU-bound loop for 3 s  (expect running/ready, R)",
        "child 1: sleep(3)                (expect waiting, S)",
        "child 2: exits immediately       (expect terminated but not reaped, Z)"
    };
    pid_t pid[NCHILD];

    printf("parent pid = %d\n", (int)getpid());
    fflush(stdout);

    for (int i = 0; i < NCHILD; i++) {
        pid[i] = fork();
        if (pid[i] < 0) { perror("fork"); return 1; }
        if (pid[i] == 0) {                       /* ---- child ---- */
            if (i == 0) {
                volatile unsigned long x = 0;
                time_t end = time(NULL) + 3;
                while (time(NULL) < end) x++;
                _exit(10);
            } else if (i == 1) {
                sleep(3);
                _exit(11);
            }
            _exit(12);                           /* i == 2 */
        }
        printf("parent: created child %d with pid %d\n", i, (int)pid[i]);
    }

    sleep(1);                                    /* let the children reach their states */

    printf("\n===== /proc/<pid>/status of each child (PCB fields) =====\n");
    for (int i = 0; i < NCHILD; i++) show_proc(pid[i], role[i]);

    printf("\n===== ps view =====\n");
    char cmd[160];
    snprintf(cmd, sizeof cmd, "ps -o pid,ppid,stat,wchan:14,comm -p %d,%d,%d",
             (int)pid[0], (int)pid[1], (int)pid[2]);
    fflush(stdout);
    if (system(cmd) == -1) perror("system");

    printf("\n===== parent reaps the children with waitpid =====\n");
    for (int i = 0; i < NCHILD; i++) {
        int st;
        waitpid(pid[i], &st, 0);
        printf("child %d (pid %d) finished, exit status %d\n",
               i, (int)pid[i], WIFEXITED(st) ? WEXITSTATUS(st) : -1);
    }
    char p2[64];
    snprintf(p2, sizeof p2, "/proc/%d", (int)pid[2]);
    printf("after waitpid, does %s still exist? %s\n", p2,
           access(p2, F_OK) == 0 ? "yes" : "no (PCB freed)");
    return 0;
}
