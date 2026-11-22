#!/bin/bash
# 5 runs of every combination. lock = mlockall; pressure = none / pageout (madvise PAGEOUT after each iteration) / hog (a memory hog in the same cgroup, MemoryHigh 200M)
cd "$(dirname "$0")" || exit 1
D=$PWD
echo "lock,pressure,iters,max_ms,p99_ms,mean_ms,misses,skipped,minflt,majflt"
for run in 1 2 3 4 5; do
  for lock in nolock lock; do
    sudo $D/rt_loop $lock none
    sudo $D/rt_loop $lock pageout
    sudo systemd-run --scope -q -p MemoryHigh=200M -p MemorySwapMax=4G bash -c "$D/hog 300 & H=\$!; sleep 3; $D/rt_loop $lock hog; kill \$H"
  done
done
