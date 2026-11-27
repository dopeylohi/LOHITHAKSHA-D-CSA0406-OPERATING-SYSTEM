#!/usr/bin/env bash
# B3.3: throughput at 3 replicas, scale to 6 WHILE load is running, throughput at 6 replicas.
set -uo pipefail
URL=http://localhost:8081/result/1921260275
tmp=$(mktemp)
bench() {
  vals=()
  for i in 1 2 3 4 5; do
    r=$(ab -q -n 30000 -c 50 "$URL" 2>&1 | awk '/Requests per second/ {print $4}')
    echo "run $i: $r req/s"; vals+=("$r")
  done
  echo "median: $(printf '%s\n' "${vals[@]}" | sort -n | sed -n 3p) req/s"
}
echo "== 3 replicas"; kubectl get pods --no-headers | wc -l | sed 's/^/pods: /'; bench
echo "== scaling to 6 while a long load run is in progress"
ab -q -n 200000 -c 50 "$URL" > "$tmp" 2>&1 &
sleep 3
kubectl scale deployment/result-portal --replicas=6
kubectl rollout status deployment/result-portal --timeout=120s
kubectl get pods
wait
echo "load run during scaling: $(awk '/Requests per second/ {print $4}' "$tmp") req/s, failed requests: $(awk '/Failed requests/ {print $3}' "$tmp")"
echo "== 6 replicas"; bench
rm -f "$tmp"
