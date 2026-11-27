# A2 - Design the system administration (5 marks)
Labels: [M] measured/built by me in Part B, [T] standard practice, [A] assumption.

## A2.1 Users, services and logs (2)
Users and groups:
 - portal: system user, no login shell, owns /opt/result-portal and runs the app (least privilege).
 - deploy: human administrator account, member of group sudo; logs in with SSH keys only.
 - dbadmin: database administrator; the database runs as its own postgres user.
 - Groups: portal-ops (may read the portal logs and restart the service through a sudoers rule) and sudo (full administration, kept to 2 or 3 people).
Services (systemd):
 - result-portal.service: User=portal, After=network.target, Restart=always, RestartSec=1, NoNewPrivileges, PrivateTmp, and limits (MemoryMax, CPUQuota) so one service cannot take the whole server [M: built in B1.3, without the limits].
 - nginx.service (reverse proxy), postgresql.service (database), cron for scheduled jobs.
Log monitoring:
 - journald keeps service logs (journalctl -u result-portal); Nginx writes access and error logs in /var/log/nginx; logrotate rotates them weekly and keeps 8 weeks.
 - A shell script counts status codes per hour and raises an alert when the 5xx share is high [M: logsummary.sh and the Nginx log count were built in B1.2 and B1.3].

## A2.2 Networking services (2)
 - IP addressing: private subnets, web tier 10.10.0.0/24 (Nginx and portal), database tier 10.10.1.0/24 (static addresses, no internet), management 10.10.9.0/24. The public address belongs to the load balancer or firewall, not to the database.
 - DNS: results.<university domain> points to the public address with a short TTL (60 s) set well before result day so traffic can be moved quickly; internal names (db.internal) for the servers.
 - Nginx reverse proxy: terminates TLS (HTTPS), forwards to gunicorn on 127.0.0.1:8000, adds X-Forwarded-For, serves static files and can rate-limit [M: proxy on port 80 built and tested in B1.3].
 - Firewall (ufw): default deny incoming, allow 80/tcp and 443/tcp from anywhere, 22/tcp only from the management subnet, 5432/tcp only from the web subnet [M: default deny plus 22, 80, 443 built; the subnet restriction was not possible on a single test machine].
 - SSH with keys: ed25519 keys, PasswordAuthentication no, PermitRootLogin no, and fail2ban for repeated failures [T].

## A2.3 Automation (1)
 Task                  | Tool                    | When
 Database backup       | pg_dump + copy off-site | every night 02:00
 Log rotation          | logrotate               | daily
 Disk-space alert      | shell script + cron     | every 30 minutes [M: disk_alert.sh, built]
 Log summary           | shell script + cron     | every hour [M: logsummary.sh, built]
 Security updates      | unattended-upgrades     | daily
 Certificate renewal   | certbot timer           | twice a day check

## Alternative concept: Ansible instead of hand-written shell scripts
 - What changes: the desired state (users, packages, files, services, firewall) is written once in a playbook. Ansible connects over SSH and only changes what differs (idempotent), so running it again is safe.
 - Time to set up (my estimates [A], not measured): shell script, 20 minutes per server by hand, so 1 server 20 min and 20 servers 20 x 20 = 400 min. Ansible: 2 hours to write the playbook once, then about 3 minutes per run; with 5 servers in parallel (the default forks is 5 [T]) 20 servers take ceil(20 / 5) x 3 = 12 min. For 1 server Ansible is slower (120 + 3 min), for 20 servers it is much faster (132 vs 400 min).
 - Risk of differences between servers: hand-run scripts drift (someone edits one server); a playbook run regularly brings all servers back to the same state.
 - Effort to learn and maintain: shell is already known and has no extra tools; Ansible needs YAML, modules and an inventory, but the playbooks are shorter to read and review.
 Pseudocode:
   ALGORITHM Configure(servers, desiredState, forks)
     FOR EACH batch OF forks servers IN servers DO IN PARALLEL
       FOR EACH item IN desiredState DO
         IF current state of item on server differs from item THEN apply item
 - Complexity: shell by hand O(n x t) time; Ansible O(ceil(n / forks) x t) time; both need O(n) inventory space.
