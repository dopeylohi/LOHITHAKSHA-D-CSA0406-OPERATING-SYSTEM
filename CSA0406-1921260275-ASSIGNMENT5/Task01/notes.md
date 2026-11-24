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
