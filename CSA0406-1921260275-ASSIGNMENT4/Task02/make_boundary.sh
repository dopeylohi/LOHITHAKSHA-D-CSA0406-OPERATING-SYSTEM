#!/bin/bash
# Builds boundary.c from the SAME allocation functions as alloc.c (everything above main), plus a test main
cd "$(dirname "$0")" || exit 1
sed '/^int main/,$d' alloc.c > boundary.c || exit 1
cat >> boundary.c <<'EOT'
static void setup(void) {
    memset(disk, '.', BLOCKS);
    alloc_contiguous('A', 6);
    alloc_contiguous('B', 8);
    alloc_contiguous('C', 5);
    alloc_contiguous('D', 7);
    release('B');
    release('D');
}

static int largest_run(void) {
    int best = 0, run = 0;
    for (int b = 0; b < BLOCKS; b++) {
        run = (disk[b] == '.') ? run + 1 : 0;
        if (run > best) best = run;
    }
    return best;
}

static int free_blocks(void) {
    int n = 0;
    for (int b = 0; b < BLOCKS; b++) if (disk[b] == '.') n++;
    return n;
}

int main(void) {
    int index[32];
    int sizes_c[] = {13, 14};
    int sizes_i[] = {20, 21};
    setup();
    printf("Start: %.32s\n", disk);
    printf("free blocks = %d, largest free run = %d\n\n", free_blocks(), largest_run());
    for (int i = 0; i < 2; i++) {
        setup();
        int r = alloc_contiguous('E', sizes_c[i]);
        printf("contiguous E size %2d -> %3d   %.32s\n", sizes_c[i], r, disk);
    }
    for (int i = 0; i < 2; i++) {
        setup();
        int r = alloc_indexed('E', sizes_i[i], index);
        printf("indexed    E size %2d -> %3d   %.32s\n", sizes_i[i], r, disk);
    }
    return 0;
}
EOT
