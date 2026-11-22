#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
static const char ro[] = "read-only constant";
static void say(const char *s) { if (write(1, s, strlen(s)) < 0) _exit(1); }
static void handler(int sig, siginfo_t *si, void *ctx)
{
    char buf[200];
    (void)sig; (void)ctx;
    snprintf(buf, sizeof buf, "SIGSEGV delivered: fault address %p, si_code %d (%s)\n", si->si_addr, si->si_code,
             si->si_code == SEGV_MAPERR ? "SEGV_MAPERR: address not mapped" :
             si->si_code == SEGV_ACCERR ? "SEGV_ACCERR: permission violation" : "other");
    say(buf);
    signal(SIGSEGV, SIG_DFL);   /* return: the faulting instruction runs again and the kernel now kills the process */
}
static void maps_line_for(const void *addr)
{
    FILE *f = fopen("/proc/self/maps", "r");
    char line[512];
    unsigned long a = (unsigned long)addr, lo, hi;
    int found = 0;
    if (!f) return;
    while (fgets(line, sizeof line, f))
        if (sscanf(line, "%lx-%lx", &lo, &hi) == 2 && a >= lo && a < hi) { printf("mapping of this address: %s", line); found = 1; }
    if (!found) printf("no mapping contains address %p\n", addr);
    fclose(f);
}
int main(int argc, char **argv)
{
    const char *what = argc > 1 ? argv[1] : "null";
    int use_handler = argc > 2 && !strcmp(argv[2], "h");
    volatile char *p;
    if (use_handler) {
        struct sigaction sa;
        memset(&sa, 0, sizeof sa);
        sa.sa_sigaction = handler;
        sa.sa_flags = SA_SIGINFO;
        sigaction(SIGSEGV, &sa, NULL);
    }
    if (!strcmp(what, "ro")) p = (volatile char *)ro; else p = (volatile char *)0;
    printf("test '%s': writing one byte to address %p\n", what, (const void *)p);
    maps_line_for((const void *)p);
    fflush(stdout);
    p[0] = 'X';
    printf("not reached\n");
    return 0;
}
