#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "usage: %s src dst\n", argv[0]); return 1; }
    int in = open(argv[1], O_RDONLY);
    if (in < 0) { perror("open src"); return 1; }
    struct stat st;
    if (fstat(in, &st) < 0) { perror("fstat"); return 1; }
    int out = open(argv[2], O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (out < 0) { perror("open dst"); return 1; }
    if (st.st_size == 0) { printf("empty file\n"); return 0; }
    if (ftruncate(out, st.st_size) < 0) { perror("ftruncate"); return 1; }
    void *s = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, in, 0);
    void *d = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, out, 0);
    if (s == MAP_FAILED || d == MAP_FAILED) { perror("mmap"); return 1; }
    memcpy(d, s, st.st_size);
    munmap(s, st.st_size); munmap(d, st.st_size);
    close(in); close(out);
    return 0;
}
