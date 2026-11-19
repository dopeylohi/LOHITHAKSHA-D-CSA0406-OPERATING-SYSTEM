#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
int main(int argc, char **argv)
{
    size_t mb = argc > 1 ? (size_t)atol(argv[1]) : 64;
    long ps = sysconf(_SC_PAGESIZE);
    struct rusage a, b;
    getrusage(RUSAGE_SELF, &a);
    volatile char *p = malloc(mb << 20);
    if (!p) { perror("malloc"); return 1; }
    for (size_t i = 0; i < (mb << 20); i += (size_t)ps) p[i] = 1;
    getrusage(RUSAGE_SELF, &b);
    printf("page size %ld bytes, touched %zu MB = %zu pages\n", ps, mb, (mb << 20) / (size_t)ps);
    printf("minor faults during touching: %ld, major faults: %ld\n", b.ru_minflt - a.ru_minflt, b.ru_majflt - a.ru_majflt);
    return 0;
}
