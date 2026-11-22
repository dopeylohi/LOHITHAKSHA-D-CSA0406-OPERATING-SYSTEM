#!/bin/bash
set -e
cd "$(dirname "$0")"
set -x
for f in touch2 memgrow cow fault_curve; do gcc -std=c11 -Wall -Wextra -O2 -o $f $f.c; done
