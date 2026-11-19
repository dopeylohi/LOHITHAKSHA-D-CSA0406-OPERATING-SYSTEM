#!/bin/bash
set -e
cd "$(dirname "$0")"
W="-std=c11 -Wall -Wextra -O2"
sed 's/#define PAGE_SIZE 1024/#define PAGE_SIZE 2048/' paging.c > paging_2048.c
set -x
for f in paging paging_2048 edge_test frag touch; do gcc $W -o $f $f.c; done
