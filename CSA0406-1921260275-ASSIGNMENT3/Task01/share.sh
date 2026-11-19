#!/bin/bash
# Two copies of each program run together; show page sharing from /proc/PID/smaps
cd "$(dirname "$0")" || exit 1
for prog in app_static app_dyn; do
  ./$prog wait > /dev/null & p1=$!
  ./$prog wait > /dev/null & p2=$!
  sleep 1
  echo "===== $prog : two copies, PIDs $p1 and $p2 ====="
  for p in $p1 $p2; do
    echo "--- PID $p ---"
    awk '/^[0-9a-f]+-[0-9a-f]+ /{show=($2 ~ /r-x/ && ($6 ~ /app_|libc\.so|libbankenc/)); if(show) printf "%s %s\n",$2,$6; next}
         show && /^(Size|Rss|Pss|Shared_Clean|Shared_Dirty|Private_Clean):/{printf "   %-14s %s kB\n",$1,$2}' /proc/$p/smaps
  done
  wait $p1 $p2
done
