#!/bin/bash
set -e
cd "$(dirname "$0")"
set -x
for f in seg maps_demo segv frag_seg; do gcc -std=c11 -Wall -Wextra -O2 -o $f $f.c; done
