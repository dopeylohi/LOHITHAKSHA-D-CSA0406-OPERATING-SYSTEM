#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc != 4) { fprintf(stderr, "usage: %s src dst bufsize\n", argv[0]); return 1; }
    size_t bufsize = strtoul(argv[3], NULL, 10);
    char *buf = malloc(bufsize);
    if (!buf) { perror("malloc"); return 1; }
    int in = open(argv[1], O_RDONLY);
    if (in < 0) { perror("open src"); return 1; }
    int out = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (out < 0) { perror("open dst"); close(in); return 1; }
    long reads = 0;
    ssize_t n;
    while ((n = read(in, buf, bufsize)) > 0) {
        reads++;
        if (write(out, buf, n) != n) { perror("write"); return 1; }
    }
    printf("buffer %zu: %ld read() calls\n", bufsize, reads);
    close(in); close(out); free(buf);
    return 0;
}
