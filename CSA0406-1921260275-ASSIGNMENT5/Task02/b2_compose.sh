#!/usr/bin/env bash
# B2: portal + database with Docker Compose; the database data lives in a named volume.
set -euo pipefail
cd "$(dirname "$0")"
docker rm -f portal >/dev/null 2>&1 || true
docker compose up -d --build
until docker compose exec -T db pg_isready -U postgres >/dev/null 2>&1; do sleep 1; done
sleep 2
echo "--- compose ps";            docker compose ps
echo "--- portal reaches db?";    curl -s http://localhost:8000/dbcheck
echo "--- write a row to the db"
docker compose exec -T db psql -U postgres -d portal -c "CREATE TABLE IF NOT EXISTS results(reg text, os int); INSERT INTO results VALUES ('1921260275', 64);"
echo "--- docker compose down (containers removed, volume kept)"
docker compose down
docker volume ls | grep dbdata
echo "--- docker compose up again"
docker compose up -d
until docker compose exec -T db pg_isready -U postgres >/dev/null 2>&1; do sleep 1; done
sleep 2
echo "--- row still there after the containers were recreated?"
docker compose exec -T db psql -U postgres -d portal -c "SELECT * FROM results;"
docker compose down
