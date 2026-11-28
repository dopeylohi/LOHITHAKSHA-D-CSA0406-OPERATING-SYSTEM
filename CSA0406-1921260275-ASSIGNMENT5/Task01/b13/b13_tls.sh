#!/bin/bash
# B1.3 extra: HTTPS (TLS) in front of the portal with a self-signed certificate
set -e
sudo mkdir -p /etc/nginx/tls
sudo openssl req -x509 -nodes -newkey rsa:2048 -days 365 \
  -keyout /etc/nginx/tls/portal.key -out /etc/nginx/tls/portal.crt \
  -subj "/C=IN/O=SIMATS/CN=result-portal.local" 2>/dev/null
sudo chmod 600 /etc/nginx/tls/portal.key
sudo tee /etc/nginx/conf.d/result-portal-tls.conf >/dev/null <<'CONF'
server {
    listen 8443 ssl;
    server_name _;
    ssl_certificate     /etc/nginx/tls/portal.crt;
    ssl_certificate_key /etc/nginx/tls/portal.key;
    ssl_protocols       TLSv1.2 TLSv1.3;
    location / {
        proxy_pass http://127.0.0.1:8000;
        proxy_set_header Host $host;
        proxy_set_header X-Forwarded-Proto https;
    }
}
CONF
sudo nginx -t
sudo systemctl reload nginx
sudo ufw allow 8443/tcp
echo "--- HTTPS request (curl -k, self-signed) ---"
curl -sk -o /dev/null -w "HTTP %{http_code} via %{scheme}\n" https://127.0.0.1:8443/health
curl -sk https://127.0.0.1:8443/health; echo
echo "--- certificate and protocol ---"
echo | openssl s_client -connect 127.0.0.1:8443 2>/dev/null | grep -E "subject=|Protocol|Cipher is" 
