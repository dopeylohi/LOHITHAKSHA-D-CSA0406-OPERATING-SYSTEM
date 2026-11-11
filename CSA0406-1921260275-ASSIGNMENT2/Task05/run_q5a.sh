#!/bin/bash
# 100 runs of each configuration; counts runs where every item was consumed exactly once and the sum is right
B="$(dirname "$0")/bb_multi"
IPP=20000
printf "%-14s %s\n" "config" "result of 100 runs (items per producer = $IPP)"
for cfg in "sem 1 1" "sem 4 4" "monitor 1 1" "monitor 4 4" "nomutex 1 1" "nomutex 4 4"; do
  pass=0; fail=0; last_fail=""
  for i in $(seq 100); do
    if out=$("$B" $cfg $IPP); then pass=$((pass+1)); else fail=$((fail+1)); last_fail="$out"; fi
  done
  printf "%-14s PASS=%3d FAIL=%3d\n" "$cfg" "$pass" "$fail"
  [ -n "$last_fail" ] && echo "   last failure: $last_fail"
done
exit 0
