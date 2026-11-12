#!/bin/bash
# 5 runs per configuration: throughput (bookings per second) and the worst oversell seen
B="$(dirname "$0")/booking"
tmp=$(mktemp)
printf "%-7s %7s %11s %11s %11s %12s\n" lock threads min/s median/s max/s oversold_max
"$B" global 1 > /dev/null                               # warm-up (not counted)
for mode in none global train coach; do
  for n in 1 4 8 16; do
    : > "$tmp"; os=0
    for i in 1 2 3 4 5; do
      out=$("$B" "$mode" "$n")
      echo "$out" | sed -n 's/.*throughput=\([0-9]*\).*/\1/p' >> "$tmp"
      o=$(echo "$out" | sed -n 's/.*oversold=\(-\{0,1\}[0-9]*\).*/\1/p')
      [ "$o" -gt "$os" ] && os=$o
    done
    read -r mn md mx <<< "$(sort -n "$tmp" | awk '{a[NR]=$1} END{printf "%d %d %d", a[1], a[3], a[5]}')"
    printf "%-7s %7s %11s %11s %11s %12s\n" "$mode" "$n" "$mn" "$md" "$mx" "$os"
  done
done
rm -f "$tmp"
exit 0
