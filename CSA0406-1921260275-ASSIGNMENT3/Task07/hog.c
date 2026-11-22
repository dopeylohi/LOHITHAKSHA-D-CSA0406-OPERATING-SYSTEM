#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
int main(int argc, char **argv)
{
    size_t mb = argc > 1 ? (size_t)atol(argv[1]) : 300;
    long ps = sysconf(_SC_PAGESIZE);
    size_t bytes = mb << 20;
    volatile char *p = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) { perror("mmap"); return 1; }
    alarm(90);                                   /* safety: the hog ends by itself after 90 s */
    for (;;) for (size_t i = 0; i < bytes; i += (size_t)ps) p[i]++;
}
