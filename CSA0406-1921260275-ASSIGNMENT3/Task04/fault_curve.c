#include <stdio.h>
#define N 256                    /* matrix is N x N ints [A] */
#define PAGE 4096
#define NREF (N * N)
#define MAXF 80
static int find(const int f[], int n, int page) { for (int i = 0; i < n; i++) if (f[i] == page) return i; return -1; }
static int fifo(const int *ref, int len, int frames)
{
    int f[MAXF], used = 0, next = 0, faults = 0;
    for (int i = 0; i < len; i++) {
        if (find(f, used, ref[i]) >= 0) continue;
        faults++;
        if (used < frames) f[used++] = ref[i]; else { f[next] = ref[i]; next = (next + 1) % frames; }
    }
    return faults;
}
static int lru(const int *ref, int len, int frames)
{
    int f[MAXF], last[MAXF], used = 0, faults = 0;
    for (int i = 0; i < len; i++) {
        int k = find(f, used, ref[i]);
        if (k >= 0) { last[k] = i; continue; }
        faults++;
        if (used < frames) { f[used] = ref[i]; last[used] = i; used++; }
        else { int v = 0; for (int j = 1; j < frames; j++) if (last[j] < last[v]) v = j; f[v] = ref[i]; last[v] = i; }
    }
    return faults;
}
int main(void)
{
    static int row[NREF], col[NREF];
    int n = 0;
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) row[n++] = (int)(((long)i * N + j) * (long)sizeof(int) / PAGE);   /* a[i][j], j inner */
    n = 0;
    for (int j = 0; j < N; j++) for (int i = 0; i < N; i++) col[n++] = (int)(((long)i * N + j) * (long)sizeof(int) / PAGE);   /* a[i][j], i inner */
    printf("# matrix %d x %d ints = %d KB = %d pages of %d bytes; %d references per traversal\n", N, N, N * N * 4 / 1024, N * N * 4 / PAGE, PAGE, NREF);
    printf("traversal,frames,LRU,FIFO\n");
    for (int fr = 1; fr <= 72; fr++) printf("row-major,%d,%d,%d\n", fr, lru(row, NREF, fr), fifo(row, NREF, fr));
    for (int fr = 1; fr <= 72; fr++) printf("column-major,%d,%d,%d\n", fr, lru(col, NREF, fr), fifo(col, NREF, fr));
    return 0;
}
