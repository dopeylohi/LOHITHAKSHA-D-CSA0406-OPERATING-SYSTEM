#!/bin/bash
# correctness check, then median of 5 runs per case
B="$(dirname "$0")/banker_bench"
med() { for i in 1 2 3 4 5; do "$B" "$@" | sed -n 's/.*time_per_check=\([0-9.]*\) us.*/\1/p'; done | sort -n | awk '{a[NR]=$1} END{print a[3]}'; }
echo "correctness check:"; "$B" check
printf "\n%-6s %6s %4s %20s\n" case n m median_us_per_check
for c in best worst; do for n in 10 100 1000; do printf "%-6s %6s %4s %20s\n" "$c" "$n" 3 "$(med "$c" "$n" 3)"; done; done
for m in 6 12; do printf "%-6s %6s %4s %20s\n" worst 1000 "$m" "$(med worst 1000 "$m")"; done
exit 0
