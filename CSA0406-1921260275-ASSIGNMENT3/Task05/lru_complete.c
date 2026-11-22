#include <stdio.h>
#define MAXF 8
int find(const int f[], int n, int page)
{
 for (int i = 0; i < n; i++) if (f[i] == page) return i;
 return -1;
}
int fifo(const int ref[], int len, int frames)
{
 int f[MAXF], used = 0, next = 0, faults = 0;
 for (int i = 0; i < len; i++) {
 if (find(f, used, ref[i]) >= 0) continue;
 faults++;
 if (used < frames) f[used++] = ref[i];
 else { f[next] = ref[i]; next = (next + 1) % frames; }
 }
 return faults;
}
int lru(const int ref[], int len, int frames)
{
 int f[MAXF], last[MAXF], used = 0, faults = 0;
 for (int i = 0; i < len; i++) {
 int k = find(f, used, ref[i]);
 if (k >= 0) { last[k] = i; continue; }
 faults++;
 // TODO 1: If a frame is free use it; otherwise replace the frame whose last use
 // (last[]) is the oldest. Record last[] = i for the frame used
 if (used < frames) { f[used] = ref[i]; last[used] = i; used++; }
 else {
 int v = 0;
 for (int j = 1; j < frames; j++) if (last[j] < last[v]) v = j;
 f[v] = ref[i]; last[v] = i;
 }
 }
 return faults;
}
int optimal(const int ref[], int len, int frames)
{
 int f[MAXF], used = 0, faults = 0;
 for (int i = 0; i < len; i++) {
 if (find(f, used, ref[i]) >= 0) continue;
 faults++;
 if (used < frames) { f[used++] = ref[i]; continue; }
 int victim = 0, farthest = -1;
 for (int j = 0; j < frames; j++) {
 int next = len; /* never used again */
 for (int t = i + 1; t < len; t++) if (ref[t] == f[j]) { next = t; break; }
 if (next > farthest) { farthest = next; victim = j; }
 }
 f[victim] = ref[i];
 }
 return faults;
}
int main(void)
{
 int ref[] = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1};
 int len = 20;
 for (int fr = 3; fr <= 4; fr++)
 printf("%d frames: FIFO %d, LRU %d, Optimal %d\n", fr, fifo(ref, len, fr),
 lru(ref, len, fr), optimal(ref, len, fr));
 int b[] = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
 printf("Belady string, FIFO: 3 frames %d faults, 4 frames %d faults\n",
 fifo(b, 12, 3), fifo(b, 12, 4));
 return 0;
}
