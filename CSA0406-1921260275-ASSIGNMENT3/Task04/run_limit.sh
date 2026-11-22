#!/bin/bash
# Run memgrow under a memory limit (cgroup v2 through systemd-run). Usage: ./run_limit.sh [limit in MB, default 256]
cd "$(dirname "$0")" || exit 1
L=${1:-256}
echo "# memory limit ${L} MB, swap allowed up to 4G, sizes in MB, 3 runs each"
echo "# --- machine state ---"; free -m; swapon --show; echo "# --- check that the limit is applied ---"
sudo systemd-run --scope -q -p MemoryMax=${L}M -p MemorySwapMax=4G ./memgrow 16 seq || { echo "FAILED: systemd-run could not apply the limit (rc=$?)"; exit 1; }
echo "MB,mode,first_ms,pass1_ms,pass2_ms,pass3_ms,minflt,majflt"
for N in 128 192 256 320 384 512; do
  for mode in seq rand; do
    for r in 1 2 3; do
      out=$(sudo timeout 300 systemd-run --scope -q -p MemoryMax=${L}M -p MemorySwapMax=4G ./memgrow $N $mode 2>/dev/null); rc=$?
      if [ $rc -eq 0 ]; then echo "$out"; else echo "$N,$mode,FAILED rc=$rc (137 = killed by the out-of-memory killer, 124 = timeout)"; fi
    done
  done
done
echo "# --- baseline without any limit ---"
for mode in seq rand; do for r in 1 2 3; do ./memgrow 512 $mode 2>/dev/null; done; done
