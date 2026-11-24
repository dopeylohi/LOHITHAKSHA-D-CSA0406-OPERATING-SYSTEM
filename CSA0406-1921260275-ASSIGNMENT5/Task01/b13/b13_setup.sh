#!/usr/bin/env bash
# B1.3: run the portal as a systemd service behind Nginx, firewall = SSH, HTTP, HTTPS only.
set -euo pipefail
D="$(cd "$(dirname "$0")" && pwd)"

sudo apt-get install -y nginx ufw python3-venv openssh-server

id portal >/dev/null 2>&1 || sudo useradd --system --home-dir /opt/result-portal --shell /usr/sbin/nologin portal
sudo mkdir -p /opt/result-portal
sudo cp "$D/app.py" "$D/requirements.txt" /opt/result-portal/
sudo python3 -m venv /opt/result-portal/venv
sudo /opt/result-portal/venv/bin/pip install -r /opt/result-portal/requirements.txt
sudo chown -R portal:portal /opt/result-portal

sudo cp "$D/result-portal.service" /etc/systemd/system/result-portal.service
sudo systemctl daemon-reload
sudo systemctl enable --now result-portal

sudo cp "$D/result-portal.nginx.conf" /etc/nginx/sites-available/result-portal
sudo ln -sf /etc/nginx/sites-available/result-portal /etc/nginx/sites-enabled/result-portal
sudo rm -f /etc/nginx/sites-enabled/default
sudo nginx -t
sudo systemctl enable --now nginx
sudo systemctl reload nginx

sudo systemctl enable --now ssh

sudo ufw default deny incoming
sudo ufw default allow outgoing
sudo ufw allow 22/tcp
sudo ufw allow 80/tcp
sudo ufw allow 443/tcp
sudo ufw --force enable
echo "B1.3 setup finished"
