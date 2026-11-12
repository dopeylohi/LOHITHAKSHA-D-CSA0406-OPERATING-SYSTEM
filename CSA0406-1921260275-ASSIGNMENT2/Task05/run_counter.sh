#!/bin/bash
# 5 runs per configuration: median time per update and median CPU time / wall time
C="$(dirname "$0")/counter"
t1=$(mktemp); t2=$(mktemp)
med() { sort -n "$1" | awk '{a[NR]=$1} END{print a[3]}'; }
printf "%-6s %-7s %7s %15s %9s %s\n" cs mode threads "ns/update(med)" cpu/wall check
run() {
  label=$1; cs=$2; total=$3; mode=$4; n=$5; : > "$t1"; : > "$t2"; ok=OK
  for i in 1 2 3 4 5; do
    out=$("$C" "$mode" "$n" "$total" "$cs")
    echo "$out" | sed -n 's/.*ns_per_update=\([0-9.]*\).*/\1/p' >> "$t1"
    echo "$out" | sed -n 's/.*cpu_over_wall=\([0-9.]*\).*/\1/p' >> "$t2"
    case "$out" in *WRONG*) ok=WRONG;; esac
  done
  printf "%-6s %-7s %7s %15s %9s %s\n" "$label" "$mode" "$n" "$(med "$t1")" "$(med "$t2")" "$ok"
}
for n in 1 4 16; do for m in mutex spin atomic; do run short 0 2000000 "$m" "$n"; done; done
for n in 1 4 16; do for m in mutex spin; do run long 20000 4000 "$m" "$n"; done; done
rm -f "$t1" "$t2"
exit 0
