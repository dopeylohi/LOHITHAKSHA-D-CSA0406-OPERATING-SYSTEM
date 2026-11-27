#!/usr/bin/env bash
# B4.1: portal + a second service (lms, an nginx stand-in for the learning-management system) + db, each with its own limits.
set -uo pipefail
cd "$(dirname "$0")"
docker compose up -d
sleep 8
echo "--- docker compose ps"; docker compose ps
echo "--- both services answer"
curl -s http://localhost:8000/health; echo
curl -sI http://localhost:8090 | head -1
echo "--- limits set on each container"
docker inspect -f '{{.Name}} NanoCpus={{.HostConfig.NanoCpus}} Memory={{.HostConfig.Memory}}' $(docker compose ps -q)
echo "--- docker stats while the portal is under load (lms and db idle)"
ab -q -n 40000 -c 20 http://localhost:8000/result/1921260275 > /dev/null 2>&1 &
sleep 3
docker stats --no-stream
wait
docker compose down
