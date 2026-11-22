#!/bin/bash
# Task 4.4: I/O scheduler of every real disk on this machine (loop and ram devices skipped)
for d in /sys/block/*; do
  case "$d" in */loop*|*/ram*) continue ;; esac
  echo "$d: scheduler=$(cat "$d/queue/scheduler" 2>&1)  rotational=$(cat "$d/queue/rotational" 2>&1)"
done
echo
lsblk -d -e 7 -o NAME,SIZE,ROTA,TYPE
