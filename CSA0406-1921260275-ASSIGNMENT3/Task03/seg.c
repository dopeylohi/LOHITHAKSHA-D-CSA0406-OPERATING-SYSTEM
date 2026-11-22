#include <stdio.h>
#define NSEG 4
struct seg { const char *name; unsigned base, limit; };
/* Segment table: base = start in physical memory, limit = length of the segment */
static const struct seg table[NSEG] = {
    {"code ", 2000, 800}, {"data ", 7200, 500}, {"stack", 5000, 300}, {"heap ", 3000, 1200}
};
int main(void)
{
    unsigned in[8][2] = {{0, 0}, {0, 799}, {0, 800}, {1, 120}, {2, 299}, {3, 1000}, {3, 1500}, {4, 10}};
    for (int i = 0; i < 8; i++) {
        unsigned s = in[i][0], d = in[i][1];
        printf("(%u, %u) -> ", s, d);
        if (s >= NSEG) printf("TRAP: invalid segment number %u\n", s);
        else if (d >= table[s].limit)
            printf("TRAP: offset %u >= limit %u of segment %u (%s), addressing error\n", d, table[s].limit, s, table[s].name);
        else printf("physical %u + %u = %u  (segment %u %s)\n", table[s].base, d, table[s].base + d, s, table[s].name);
    }
    return 0;
}
