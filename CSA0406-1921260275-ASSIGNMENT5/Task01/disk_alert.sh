#!/usr/bin/env bash
# Disk-space alert: warn when any real filesystem is at or above THRESHOLD percent used.
# Usage: disk_alert.sh [threshold_percent]   (default 80)
# /usr/src/* is skipped: on WSL2 it is a read-only kernel-headers image that is always 100% full.
set -euo pipefail
threshold="${1:-80}"
alerts=0
while read -r fs pct mp; do
  use="${pct%\%}"
  if [ "$use" -ge "$threshold" ]; then
    echo "$(date '+%F %T') ALERT: $mp ($fs) is ${use}% used (threshold ${threshold}%)"
    alerts=$((alerts + 1))
  fi
done < <(df -P -x tmpfs -x devtmpfs -x squashfs -x overlay | awk 'NR>1 && $6 !~ /^\/usr\/src\// {print $1, $5, $6}')
if [ "$alerts" -eq 0 ]; then
  echo "$(date '+%F %T') OK: no filesystem at or above ${threshold}%"
fi
