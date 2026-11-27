#!/usr/bin/env bash
# B3.1/B3.2: same app (Task02/app.py, 1 gunicorn worker) on bare metal, in a KVM VM and in a Docker container.
# Measures start-up time (5 runs), memory, and requests/second (ab, 5 runs); medians reported.
set -uo pipefail
R=${R:?R is not set}
APP="$R/Task02"; VM="$R/Task03/vm"
VENV=/opt/result-portal/venv/bin/gunicorn
N=5

start_bare()      { "$VENV" --workers 1 --bind 127.0.0.1:8001 --chdir "$APP" app:app >/dev/null 2>&1 & PID=$!; }
stop_bare()       { kill "$PID" 2>/dev/null; wait "$PID" 2>/dev/null; }
URL_bare=http://127.0.0.1:8001
start_container() { docker run -d --rm --name portal-cmp -p 8000:8000 result-portal >/dev/null; }
stop_container()  { docker rm -f portal-cmp >/dev/null 2>&1; }
URL_container=http://127.0.0.1:8000
start_vm()        { qemu-system-x86_64 -enable-kvm -cpu host -m 1024 -smp 2 \
                      -drive file="$VM/vm.qcow2",if=virtio -drive file="$VM/seed.img",format=raw,if=virtio,readonly=on \
                      -snapshot -nic user,hostfwd=tcp::8082-:8000 -display none -serial none -monitor none >/dev/null 2>&1 & PID=$!; }
stop_vm()         { kill "$PID" 2>/dev/null; wait "$PID" 2>/dev/null; }
URL_vm=http://127.0.0.1:8082

mem_bare()      { ps -o rss= -p "$(pgrep -d, -f 'bind 127.0.0.1:8001')" | awk '{s += $1} END {printf "%.1f MiB (sum of RSS of %d gunicorn processes)", s/1024, NR}'; }
mem_container() { docker stats --no-stream --format '{{.MemUsage}}' portal-cmp; }
mem_vm()        { ps -o rss= -p "$PID" | awk '{printf "%.0f MiB (RSS of the QEMU process on the host; guest configured with 1024 MiB)", $1/1024}'; }

wait_ready() {  # $1 = url, $2 = start time; returns 0 when /health answers, 1 after 180 s
  while ! curl -sf -o /dev/null -m 1 "$1/health"; do
    sleep 0.05
    awk -v s="$2" -v n="$(date +%s.%N)" 'BEGIN {exit !(n - s > 180)}' && return 1
  done
  return 0
}

for mode in bare container vm; do
  echo "=== $mode ==="
  uv=URL_$mode; url=${!uv}
  times=()
  for i in $(seq 1 $N); do
    s=$(date +%s.%N); "start_$mode"
    if wait_ready "$url" "$s"; then
      e=$(date +%s.%N)
      t=$(awk -v s="$s" -v e="$e" 'BEGIN {printf "%.2f", e - s}')
    else
      t=TIMEOUT
    fi
    echo "start-up $i: $t s"; times+=("$t")
    "stop_$mode"; sleep 2
  done
  echo "start-up median: $(printf '%s\n' "${times[@]}" | sort -n | sed -n 3p) s"
  s=$(date +%s.%N); "start_$mode"
  if ! wait_ready "$url" "$s"; then echo "service did not start; skipping load test"; "stop_$mode"; continue; fi
  sleep 3; curl -s "$url/result/1921260275" >/dev/null
  echo "memory: $("mem_$mode")"
  vals=()
  for i in $(seq 1 $N); do
    r=$(ab -q -n 30000 -c 20 "$url/result/1921260275" 2>&1 | awk '/Requests per second/ {print $4}')
    echo "load run $i: $r req/s"; vals+=("$r")
  done
  echo "requests/second median: $(printf '%s\n' "${vals[@]}" | sort -n | sed -n 3p)"
  "stop_$mode"; sleep 2
done
echo "=== disk sizes ==="
du -sm /opt/result-portal/venv | awk '{print "bare metal: Python packages in the virtualenv = " $1 " MiB (the OS python3 itself is shared)"}'
du -h "$VM/base.img" "$VM/vm.qcow2"
