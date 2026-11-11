#!/bin/bash
# 5 timed runs per configuration; prints min / median / max wall time and speed-up vs sequential median
P="$(dirname "$0")/plates"
tmp=$(mktemp)
base=""
stats() { sort -n "$1" | awk '{a[NR]=$1} END{m=(NR%2)?a[(NR+1)/2]:(a[NR/2]+a[NR/2+1])/2; printf "%.1f %.1f %.1f", a[1], m, a[NR]}'; }
printf "%-24s %9s %9s %9s %8s  %s\n" config min_ms median_ms max_ms speedup checksum
run() {
  label="$1"; shift; : > "$tmp"; ck=""
  for i in 1 2 3 4 5; do
    out=$("$P" "$@")
    echo "$out" | sed -n 's/.*wall=\([0-9.]*\) ms.*/\1/p' >> "$tmp"
    ck=$(echo "$out" | sed -n 's/.*checksum=\([0-9]*\).*/\1/p')
  done
  read -r mn md mx <<< "$(stats "$tmp")"
  [ -z "$base" ] && base=$md
  sp=$(awk -v b="$base" -v m="$md" 'BEGIN{printf "%.2f", b/m}')
  printf "%-24s %9s %9s %9s %8s  %s\n" "$label" "$mn" "$md" "$mx" "$sp" "$ck"
}
"$P" seq > /dev/null                                  # warm-up run (not counted)
run "sequential (1 thread)" seq
run "pool of 2 threads" pool 2
run "pool of 4 threads" pool 4
run "pool of 8 threads" pool 8
run "pool of 16 threads" pool 16
run "1 thread per camera (64)" pool 64
run "4 processes, shm" procs 4 shm
run "8 processes, shm" procs 8 shm
run "8 processes, pipe" procs 8 pipe
rm -f "$tmp"
