#!/usr/bin/env bash
# Alternative concept: size, build time (5 runs, --no-cache, median) and package count of 3 image variants.
set -uo pipefail
cd "$(dirname "$0")"
docker pull -q python:3.12-slim >/dev/null
docker pull -q ubuntu:24.04 >/dev/null
for v in slim:Dockerfile multistage:Dockerfile.multistage ubuntu:Dockerfile.ubuntu; do
  tag=${v%%:*}; file=${v#*:}; times=()
  for i in 1 2 3 4 5; do
    s=$(date +%s.%N)
    docker build --network=host --no-cache -q -f "$file" -t "result-portal:$tag" . >/dev/null || { echo "build failed: $file"; exit 1; }
    e=$(date +%s.%N)
    t=$(awk -v s="$s" -v e="$e" 'BEGIN {printf "%.1f", e - s}')
    echo "$tag build $i: $t s"; times+=("$t")
  done
  echo "$tag build median: $(printf '%s\n' "${times[@]}" | sort -n | sed -n 3p) s"
done
echo
docker images result-portal
for tag in slim multistage ubuntu; do
  echo "$tag packages (dpkg ii): $(docker run --rm --entrypoint dpkg "result-portal:$tag" -l | grep -c '^ii')"
done
