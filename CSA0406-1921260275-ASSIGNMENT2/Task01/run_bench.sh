#!/bin/bash
B="$(dirname "$0")/bench"
for i in 1 2 3 4 5; do
  sleep 3; "$B" spawn procs 2000
  sleep 3; "$B" spawn threads 2000
done
for i in 1 2 3 4 5; do "$B" pingpong procs 200000; done
for i in 1 2 3 4 5; do "$B" pingpong threads 200000; done
