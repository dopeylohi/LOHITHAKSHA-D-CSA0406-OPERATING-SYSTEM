#!/bin/bash
# Start-up time: 5 rounds x 500 runs per program; memory: VmRSS from 5 runs. Medians reported.
cd "$(dirname "$0")" || exit 1
N=500
median() { sort -n | awk '{a[NR]=$1} END{print a[int((NR+1)/2)]}'; }
for prog in app_static app_dyn; do
  : > /tmp/t_$prog.txt
  for r in 1 2 3 4 5; do
    t0=$(date +%s%N)
    for ((i=0;i<N;i++)); do ./$prog q > /dev/null; done
    t1=$(date +%s%N)
    us=$(( (t1 - t0) / N / 1000 ))
    echo "$prog round $r: $us us per run"
    echo $us >> /tmp/t_$prog.txt
  done
  echo "$prog MEDIAN start-up: $(median < /tmp/t_$prog.txt) us per run [M]"
  for r in 1 2 3 4 5; do ./$prog mem | grep VmRSS | awk '{print $2}'; done > /tmp/m_$prog.txt
  echo "$prog VmRSS runs (kB): $(tr '\n' ' ' < /tmp/m_$prog.txt)"
  echo "$prog MEDIAN VmRSS: $(median < /tmp/m_$prog.txt) kB [M]"
done
