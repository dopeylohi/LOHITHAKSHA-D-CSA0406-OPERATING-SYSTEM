#!/usr/bin/env bash
# B2: effect of --cpus / --memory. ab sends load; docker stats is sampled during run 3; 5 runs, median.
set -uo pipefail
URL=http://localhost:8000/result/1921260275
tmp=$(mktemp)
run_case() {
  label="$1"; shift
  docker rm -f portal-test >/dev/null 2>&1
  docker run -d --name portal-test -p 8000:8000 "$@" result-portal >/dev/null || return 1
  sleep 3
  echo "=== $label: docker run flags: $* ==="
  docker inspect -f 'NanoCpus={{.HostConfig.NanoCpus}} Memory={{.HostConfig.Memory}}' portal-test
  rps=()
  for i in 1 2 3 4 5; do
    if [ "$i" -eq 3 ]; then
      ab -q -n 30000 -c 20 "$URL" > "$tmp" 2>&1 &
      sleep 2
      docker stats --no-stream portal-test
      wait
    else
      ab -q -n 30000 -c 20 "$URL" > "$tmp" 2>&1
    fi
    r=$(awk '/Requests per second/ {print $4}' "$tmp")
    f=$(awk '/Failed requests/ {print $3}' "$tmp")
    echo "run $i: $r req/s (failed requests: $f)"
    rps+=("$r")
  done
  echo "median: $(printf '%s\n' "${rps[@]}" | sort -n | sed -n 3p) req/s"
  docker rm -f portal-test >/dev/null
}
run_case unlimited
run_case limited --cpus 0.5 --memory 128m
rm -f "$tmp"
