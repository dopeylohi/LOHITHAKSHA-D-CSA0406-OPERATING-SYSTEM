#!/usr/bin/env bash
# B3.3: delete one READY pod 5 times; time until 3 other pods are Running and Ready. Median reported.
set -uo pipefail
ready_pods() { kubectl get pods -l app=result-portal --no-headers 2>/dev/null | awk -v p="${1:-}" '$1 != p && $2 == "1/1" && $3 == "Running" {print $1}'; }
times=()
for i in 1 2 3 4 5; do
  until [ "$(ready_pods | wc -l)" -ge 3 ]; do sleep 0.5; done
  pod=$(ready_pods | head -1)
  s=$(date +%s.%N)
  kubectl delete pod "$pod" --wait=false > /dev/null
  until [ "$(ready_pods "$pod" | wc -l)" -ge 3 ]; do sleep 0.2; done
  e=$(date +%s.%N)
  t=$(awk -v s="$s" -v e="$e" 'BEGIN {printf "%.1f", e - s}')
  echo "run $i: deleted $pod, 3 ready pods again after $t s"
  times+=("$t")
done
echo "median recovery: $(printf '%s\n' "${times[@]}" | sort -n | sed -n 3p) s"
