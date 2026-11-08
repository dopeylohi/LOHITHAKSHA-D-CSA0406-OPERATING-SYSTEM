#!/bin/bash
TIMEFORMAT='%R %U %S'
for b in 1 16 64 512 4096 65536 1048576; do
  for i in 1 2 3 4 5; do
    { time ./bench_copy big.dat out.dat $b >/dev/null; } 2>&1
  done | awk -v b=$b '{r+=$1;u+=$2;s+=$3; if(NR==1||$1<mn)mn=$1; if(NR==1||$1>mx)mx=$1} END{printf "buffer %8d  real mean %.3f (min %.3f max %.3f)  user %.3f  sys %.3f\n", b,r/NR,mn,mx,u/NR,s/NR}'
done
