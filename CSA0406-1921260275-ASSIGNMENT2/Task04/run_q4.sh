#!/bin/bash
# 5 timed runs per method and size; prints min / median / max transfer time and the pipe/shm ratio
X="$(dirname "$0")/xfer"
med() {
  for i in 1 2 3 4 5; do "$X" "$1" "$2" | sed -n 's/.*time= *\([0-9.]*\) ms.*/\1/p'; done |
    sort -n | awk '{a[NR]=$1} END{printf "%.3f %.3f %.3f", a[1], a[3], a[5]}'
}
printf "%-8s | %-32s | %-32s | %s\n" size "pipe min / median / max (ms)" "shm min / median / max (ms)" "pipe/shm"
for s in 1 10 100; do
  "$X" pipe "$s" > /dev/null; "$X" shm "$s" > /dev/null          # warm-up (not counted)
  read -r pmn pmd pmx <<< "$(med pipe "$s")"
  read -r smn smd smx <<< "$(med shm "$s")"
  r=$(awk -v a="$pmd" -v b="$smd" 'BEGIN{printf "%.2f", a/b}')
  printf "%4s MiB | %10s %10s %10s | %10s %10s %10s | %sx\n" "$s" "$pmn" "$pmd" "$pmx" "$smn" "$smd" "$smx" "$r"
done
echo; "$X" pipe 100; "$X" shm 100
