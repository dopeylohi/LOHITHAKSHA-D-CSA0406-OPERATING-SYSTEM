#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 4) { fprintf(stderr, "usage: %s src dst chunk\n", argv[0]); return 1; }
    size_t c = strtoul(argv[3], NULL, 10);
    char *buf = malloc(c);
    FILE *in = fopen(argv[1], "rb");
    if (!in) { perror("fopen src"); return 1; }
    FILE *out = fopen(argv[2], "wb");
    if (!out) { perror("fopen dst"); return 1; }
    size_t n;
    while ((n = fread(buf, 1, c, in)) > 0) fwrite(buf, 1, n, out);
    fclose(in); fclose(out); free(buf);
    return 0;
}
