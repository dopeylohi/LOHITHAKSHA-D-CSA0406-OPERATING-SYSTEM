# Task 01 (B1 part 1) - Observe, automate, cron

Labels: [M] measured on my WSL2 machine, [C] calculated, [T] typical/published meaning.
Machine: Ubuntu on WSL2, kernel 6.18.58.1-microsoft-standard-WSL2, 8 CPUs, 8.7 GiB RAM [M].

## B1.1 Observe the system (what each value means)

ps aux --sort=-%mem: lists processes with the biggest memory first. RSS is real RAM in use (KB), %MEM is RSS divided by total RAM.
 - Biggest process was dockerd, RSS 85492 KB = 0.9 % of RAM [M]. Next: systemd-journald 52388 KB, containerd 49572 KB [M].

top -bn1: one snapshot of the whole machine.
 - 33 tasks: 1 running, 32 sleeping, 0 zombie [M]. Zombie = finished process whose parent has not collected it; 0 is healthy.
 - CPU 100.0 % idle [M]. Load average 0.11, 0.06, 0.01 (1, 5, 15 minutes) [M]. Load average = processes running or waiting to run; 0.11 on 8 CPUs means almost no work [C: 0.11/8 = 0.014 per CPU].

free -h:
 - total 8.7Gi, used 571Mi, free 7.6Gi, buff/cache 728Mi, available 8.2Gi [M].
 - "free" is memory nothing uses. "available" is bigger because buff/cache (the page cache) can be given back to programs when needed. Available is the number that matters.
 - Swap: 4.0Gi total, 0B used [M]. Nothing has been pushed to disk.

vmstat 1 5: one line per second.
 - si and so (swap in/out) were 0 on every line [M], so no swapping. b (blocked processes) was 0. id (idle CPU) was 98 on the first line and 100 on the rest [M].
 - The first line is an average since boot, so its numbers (for example bi 4538) are not the live rate. The later lines show the live rate: about 340-390 interrupts/s and 300-350 context switches/s [M].
 - bi/bo = blocks read from / written to disk per second.

iostat -x 1 3: disk and CPU statistics.
 - Average CPU: 0.53 % user, 0.81 % system, 0.16 % iowait, 98.50 % idle [M]. iowait = CPU idle while waiting for disk.
 - sdd is the busiest disk (the root file system): 82.72 reads/s, 3691.56 kB/s read, 15.71 writes/s, %util 1.41 [M]. %util = how much of the time the disk was busy; 1.41 % means no disk bottleneck. (First report is an average since boot.)

df -h:
 - / is /dev/sdd: size 1007G, used 20G, available 937G, 2 % [M]. /mnt/c (Windows C: drive) is 84 % used [M].
 - Use% = used / size. This is what the disk alert script watches.

ss -tulpn: lists listening TCP and UDP sockets with the process that owns each (see ss.txt).

Interpretation [C]: the machine is idle and healthy. CPU is about 100 % idle, memory has about 8.2 Gi available, nothing is swapped and the disk is about 1.4 % busy. This is the baseline to compare against later when the portal is under load.

## B1.2 Automate

logsummary.sh (the Complete the Code script):
 - TODO 1: awk prints field 7 (status code), sort groups equal codes, uniq -c counts them, a second awk reformats to "  200 4".
 - TODO 2: awk prints field 1 (client IP), sort | uniq -c counts, sort -k1,1nr orders by count (highest first), awk NR==1 prints only the first line.
 - I used awk NR==1 instead of head -1 on purpose: the script has set -o pipefail, and head can close the pipe early and make the pipeline report an error.
 - Output was identical to the expected output (diff printed MATCH) [M]. Total bytes = 43200 [M].

disk_alert.sh [threshold]: reads df -P, compares each Use% with the threshold (default 80), prints an ALERT line for each filesystem at or above it, otherwise one OK line.
 - Boundary tests [M]: threshold 1 gave ALERT lines for the real filesystems; threshold 100 gave a single OK line.
 - Deviation found by testing: my first version raised an ALERT at threshold 100 for /usr/src/linux-headers-6.18.58.1-microsoft-standard-WSL2 (/dev/sdb, 100 % used). It is a small read-only image that WSL mounts and is always full by design, so I excluded /usr/src/* in the awk filter.

cron:
 - Test: an every-minute line (* * * * *) ran logsummary.sh and cron_logsummary.log held 3 complete runs after about 130 seconds [M]. This proves cron really executes the script.
 - Final crontab lines [M]:
   0 * * * * /bin/bash <full path>/Task01/logsummary.sh >> <full path>/Task01/cron_logsummary.log 2>&1
   */30 * * * * /bin/bash <full path>/Task01/disk_alert.sh 80 >> <full path>/Task01/cron_disk_alert.log 2>&1
 - Fields are minute, hour, day of month, month, day of week. "0 * * * *" = minute 0 of every hour = every hour. Full paths are used because cron has no $R and a very small PATH.
 - Note: the test log also contains the 3 test runs.

## ss -tulpn result [M]
 - Listening sockets were only local services: DNS on port 53 (127.0.0.53, 127.0.0.54 and 10.255.255.254), UDP 323 on 127.0.0.1 and [::1], and one TCP socket on 127.0.0.1:34927.
 - Nothing was listening on port 22, 80 or 443, so this is the baseline before Nginx and SSH are set up in B1.3.
 - The Process column was empty because I ran ss without sudo; [T] seeing the owner of a socket needs root.
 - Columns: Netid = protocol, State = LISTEN (waiting for connections) or UNCONN (UDP has no connection), Recv-Q/Send-Q = queued bytes or backlog, Local Address:Port = where it listens. 127.0.0.1 means reachable only from this machine.

## B1.3 Services and network [M]
Setup: b13/b13_setup.sh finished with exit code 0. Installed nginx 1.28.3, ufw, openssh-server; Flask 3.1.3 and gunicorn 27.0.1 in a virtualenv at /opt/result-portal/venv.

What was built:
 - A system user "portal" with no login shell runs the app (least privilege: if the app is hacked it is not root).
 - systemd unit result-portal.service: gunicorn with 2 workers on 127.0.0.1:8000, Restart=always, RestartSec=1, NoNewPrivileges, PrivateTmp.
 - Nginx is the reverse proxy on port 80: it receives the visitor's request and forwards it to gunicorn on 127.0.0.1:8000, adding Host, X-Real-IP and X-Forwarded-For headers. nginx -t reported "syntax is ok" and "test is successful".
 - ufw: default deny incoming, default allow outgoing, allowed 22/tcp, 80/tcp, 443/tcp (each added for IPv4 and IPv6). "Firewall is active and enabled on system startup". Full table in b13/ufw_status.txt.

Evidence:
 - systemctl status: result-portal active (running), enabled; main PID 3089 (gunicorn master) plus 2 workers (PIDs 3135, 3136); memory 58.9M (peak 59.3M). nginx active (running), main PID 2869, memory 7.6M (peak 18.3M).
 - Through Nginx on port 80: /health gave 200 and {"host":"lol","status":"ok"}; / gave "University Result Portal (demo)"; /result/1921260275 gave 200 with demo marks; /result/abc gave 400 (error: register number must be 10 digits); /nope gave 404. Response header "Server: nginx/1.28.3 (Ubuntu)" shows the request went through Nginx.
 - Nginx access log status counts after these 5 requests: 3 x 200, 1 x 400, 1 x 404. Same awk/sort/uniq idea as logsummary.sh, but on the real log where the status is field 9 (Nginx logs the request line as 3 fields).
 - ss -tulpn (as root) now shows: sshd on 0.0.0.0:22 and [::]:22; nginx on 0.0.0.0:80 (1 master + 8 workers, one per CPU); gunicorn only on 127.0.0.1:8000 (not reachable from outside, only Nginx can reach it); containerd on 127.0.0.1:34927. Before B1.3 nothing listened on 22 or 80, so this is the before/after change.
 - Failure test: I killed the gunicorn master with kill -9. Main PID changed from 3089 to 3738 within the 3 second wait, state "active", and /health returned status ok. systemd restarted it because of Restart=always.

Honest limits:
 - Nothing listens on port 443: no TLS certificate was configured, so the HTTPS rule only opens the port. In production I would add a certificate (for example Let's Encrypt) to Nginx.
 - The firewall rules were not tested from another machine. Traffic from this machine to itself uses the loopback interface, which ufw does not block, so a blocked port cannot be demonstrated here. What I can show is the active rules and that only 22 and 80 (and local-only services) are listening.
 - The app is a demo with hash-derived marks, not real student data.

## B1.4 Compare with my Part A design (A2)
 What I designed (A2) and built (B1.3):
 - Dedicated no-login user running the app: designed and built (user "portal").
 - systemd service with automatic restart: designed and built; the kill -9 test showed the restart. Memory and CPU limits in the unit: designed, not built.
 - Nginx reverse proxy on port 80 forwarding to gunicorn on 127.0.0.1: designed and built.
 - ufw default deny with 22, 80 and 443 allowed: built as designed. Change: in the design 22 is only open to the management subnet; on my single test machine there is no separate subnet, so 22 is open to anywhere.
 - HTTPS: designed (TLS in Nginx), not built: no certificate was installed and nothing listens on 443. I ran out of reason to fake it, so it is listed as a gap.
 - SSH keys only: designed; I installed sshd with its default settings and did not disable password login.
 - Database on its own server: designed; in B1.3 the portal has no database. The database appeared in Task02 (Compose) as a separate container with a volume.
 - Automation: the disk alert and log summary scripts and their cron lines were built as designed. Nightly backup, logrotate and certificate renewal were designed only.
 Reason for the changes: the build ran on one WSL2 machine for a few days, so anything that needs several machines, a subnet or a public certificate stays at design level.
