#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

/* 4 GiB file = 2^20 blocks of 4 KiB; 4-byte block numbers, 1024 pointers per block */
#define NB   (1u << 20)
#define P    1024u
#define RUNS 5

static uint32_t order[NB];       /* order[i] = disk block that holds file block i (scattered) */
static uint32_t fat[NB];         /* FAT: fat[b] = next block of the file */
static uint32_t direct[12];      /* inode: 12 direct pointers */
static uint32_t single[P];       /* single-indirect block */
static uint32_t dbl_top[P];      /* double-indirect block: points to P single-indirect blocks */
static uint32_t dbl[P][P];       /* the P single-indirect blocks under it */

static volatile uint32_t sink;   /* stops the compiler removing the loops */

/* FAT: follow k links in memory, then one disk read for the data block */
static uint32_t fat_lookup(uint32_t start, uint32_t k) {
    uint32_t b = start;
    for (uint32_t i = 0; i < k; i++) b = fat[b];
    return b;
}

/* Indexed: returns the data block of file block k; *reads = disk reads needed (inode assumed in memory) */
static uint32_t index_lookup(uint32_t k, int *reads) {
    if (k < 12) { *reads = 1; return direct[k]; }
    k -= 12;
    if (k < P)  { *reads = 2; return single[k]; }
    k -= P;
    *reads = 3;
    return dbl[dbl_top[k / P]][k % P];
}

static double now_ns(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e9 + t.tv_nsec;
}

static void sort5(double a[RUNS]) {
    for (int i = 1; i < RUNS; i++) {
        double v = a[i]; int j = i - 1;
        while (j >= 0 && a[j] > v) { a[j + 1] = a[j]; j--; }
        a[j + 1] = v;
    }
}

int main(void) {
    /* scatter the file over the disk: Fisher-Yates shuffle with a fixed seed */
    for (uint32_t i = 0; i < NB; i++) order[i] = i;
    srand(42);
    for (uint32_t i = NB - 1; i > 0; i--) {
        uint32_t j = (uint32_t)(((uint64_t)rand() * 65536u + (uint64_t)rand()) % (i + 1));
        uint32_t t = order[i]; order[i] = order[j]; order[j] = t;
    }
    /* build the FAT chain and the inode index for the same file */
    for (uint32_t i = 0; i + 1 < NB; i++) fat[order[i]] = order[i + 1];
    fat[order[NB - 1]] = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < NB; i++) {
        uint32_t k = i;
        if (k < 12) { direct[k] = order[i]; continue; }
        k -= 12;
        if (k < P) { single[k] = order[i]; continue; }
        k -= P;
        dbl_top[k / P] = k / P;
        dbl[k / P][k % P] = order[i];
    }

    uint32_t ks[3] = {100, 10000, NB - 1};
    printf("File: %u blocks (4 GiB). Times are in-memory look-ups only, warm cache. %d runs each.\n", NB, RUNS);
    printf("%-9s %-10s %-12s %-30s %-10s\n", "method", "k", "lookups", "ns per run (5 runs, sorted)", "median ns");
    for (int m = 0; m < 3; m++) {
        uint32_t k = ks[m];
        uint32_t want = order[k];
        /* consistency check: both methods must find the same data block */
        int reads = 0;
        uint32_t viaFat = fat_lookup(order[0], k);
        uint32_t viaIdx = index_lookup(k, &reads);
        if (viaFat != want || viaIdx != want) { printf("MISMATCH at k=%u\n", k); return 1; }

        long repsF = 2000000L / (long)k; if (repsF < 1) repsF = 1;
        double f[RUNS], x[RUNS];
        for (int r = 0; r < RUNS; r++) {
            double t0 = now_ns();
            for (long i = 0; i < repsF; i++) sink = fat_lookup(order[0], k);
            f[r] = (now_ns() - t0) / (double)repsF;
        }
        long repsI = 2000000L;
        for (int r = 0; r < RUNS; r++) {
            double t0 = now_ns();
            for (long i = 0; i < repsI; i++) { int rd; sink = index_lookup(k, &rd); }
            x[r] = (now_ns() - t0) / (double)repsI;
        }
        sort5(f); sort5(x);
        printf("%-9s %-10u %-12u %8.1f %8.1f %8.1f %8.1f %8.1f %10.1f\n", "FAT", k, k, f[0], f[1], f[2], f[3], f[4], f[2]);
        printf("%-9s %-10u %-12d %8.1f %8.1f %8.1f %8.1f %8.1f %10.1f\n", "index", k, reads, x[0], x[1], x[2], x[3], x[4], x[2]);
    }
    printf("lookups column: FAT = memory look-ups (+1 disk read); index = disk reads incl. the data block.\n");
    printf("consistency: FAT and index found the same data block for every k.\n");
    return 0;
}
