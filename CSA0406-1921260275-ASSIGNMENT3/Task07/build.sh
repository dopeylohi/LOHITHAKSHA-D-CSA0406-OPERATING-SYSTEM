#!/bin/bash
set -e
cd "$(dirname "$0")"
set -x
for f in rt_loop hog simulate; do gcc -std=c11 -Wall -Wextra -O2 -o $f $f.c; done
