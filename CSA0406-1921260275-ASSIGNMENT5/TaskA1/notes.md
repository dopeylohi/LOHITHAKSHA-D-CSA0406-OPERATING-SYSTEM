# A1 - Evaluate the Linux system components (7 marks)
Labels: [M] measured by me in Part B, [T] standard/published fact, [A] assumption.

## A1.1 Linux architecture (2)
Layers, bottom to top: hardware -> kernel -> system-call interface -> system libraries -> shell and utilities -> applications (gunicorn, Nginx).
 - Kernel: the privileged core that alone touches the hardware. It is monolithic (drivers and file systems run inside it) but can load modules.
 - System libraries: glibc turns calls such as open() or socket() into system calls that enter the kernel. Programs rarely call the kernel directly.
 - Shell and utilities: bash reads commands and starts programs; ps, free, df, ss, awk are small programs that read kernel information. systemd is process 1 and starts the services [M: PID 1 was systemd in top].
How the kernel manages:
 - Processes: creates them (fork, exec), schedules CPU time, delivers signals and provides communication between them. [M] 33 tasks on my idle machine; a kill -9 on gunicorn was handled by systemd restarting it.
 - Memory: each process gets a private virtual address space mapped to RAM by page tables; the kernel keeps a page cache, swaps when short of RAM and kills a process with the OOM killer in the worst case [M: a container breaching its 20m limit was killed, exit code 137].
 - Files: the virtual file system (VFS) gives every file system the same interface; the page cache buffers disk data.
 - Devices: drivers hide the hardware behind files in /dev; interrupts tell the kernel when hardware needs attention [M: iostat showed the disks sda to sdd].
 - Network: the socket interface and the TCP/IP stack; netfilter does the firewall work behind ufw [M: ss -tulpn listed the listening sockets].

## A1.2 Process and memory management (3)
Scheduler (CFS):
 - Each runnable task has a virtual runtime (vruntime) that grows while it runs. The scheduler always runs the task with the smallest vruntime, kept in a red-black tree, so picking is cheap (O(log n) to insert, the leftmost task is cached) and every task gets a fair share. Nice values change the weight. [T]
 - Newer kernels (6.6 and later, including my WSL kernel 6.18) replaced CFS with EEVDF, which keeps the same fairness idea and adds deadlines for better latency [T].
 - Evaluation: strengths are fairness and good scaling with many tasks. Weaknesses: it is not a real-time scheduler, and it gives no guarantee between containers unless cgroup limits are set. Containers use this scheduler: docker --cpus 0.5 is a CFS quota. [M] A container limited to 0.5 CPU used 49.69 % of a core and served 726.46 req/s against 1295.90 req/s unlimited (78.87 % of a core).
Page cache and swapping:
 - The page cache keeps recently used file data in free RAM, so repeated reads do not touch the disk. [M] free showed 728Mi buff/cache and 8.2Gi available but only 7.6Gi free: the cache can be given back, so "available" is the number to watch.
 - Swapping writes rarely used anonymous pages to disk when RAM is short. It is slow, so heavy swapping makes a server crawl. [M] swap used was 0B and vmstat si/so were 0, so there was no memory pressure.
 - Evaluation for result day: size RAM for the peak, set container memory limits so one service cannot starve the others, and watch si/so in vmstat as the early warning.
/proc and /sys:
 - /proc is a virtual file system showing processes and kernel state (/proc/meminfo, /proc/loadavg, /proc/<pid>/status). ps, top and free read it. /sys exposes devices and kernel settings, and cgroup limits live under /sys/fs/cgroup.
 - Evaluation: free to read, scriptable and always current, but the values are raw and need interpretation, and /proc shows only the point in time of the read [M: my observe.sh used these tools].

## A1.3 File system and permissions (2)
 - Directory hierarchy: one tree from /. /etc configuration, /var logs and variable data, /usr programs, /home users, /opt added software, /dev devices, /proc and /sys virtual, /tmp temporary. [M] I used /etc/nginx, /etc/systemd/system, /opt/result-portal and /var/log/nginx for the portal.
 - VFS: one set of calls (open, read, write) works on ext4, tmpfs, overlayfs and the Windows drive. [M] df showed / on /dev/sdd and /mnt/c (C:\) side by side.
 - Permissions: each file has an owner, a group and others, each with read, write and execute. chmod changes the bits and chown the owner. setuid makes a program run with the file owner's rights (passwd needs this); it is useful but dangerous, so few files should have it. sudo lets chosen users run chosen commands as root and logs them.
 - Evaluation: simple and fast, but only three classes; ACLs are needed for finer rules. For the portal the app runs as a no-login system user "portal", not root [M: whoami in the container printed portal].

## Alternative concept: Windows Server instead of Linux
 - Memory and disk of the base system [T: published minimums, to be checked on the vendor pages before quoting]: Windows Server needs about 512 MB RAM for Server Core and about 2 GB with the desktop, and about 32 GB of disk; a minimal Ubuntu Server needs about 1 GB RAM and a few GB of disk. [M] On my side the Ubuntu cloud image I used for the VM was 593M, and an idle Linux machine with Docker used 571Mi of RAM.
 - Time to automate administration: both can be scripted (bash and cron on Linux; PowerShell and Task Scheduler on Windows). I did not build the Windows version, so I give no time measurement. Linux needs only shell tools that are already installed; Windows needs PowerShell skill.
 - Licence cost per server: Linux has no per-server licence fee (support is optional). Windows Server is licensed per core, with extra client access licences [T]. I did not look up prices, so I give no figure.
 - Complexity: with n servers, licence cost grows as O(n) for Windows and stays O(1) per server (zero) for Linux; script effort per server is the same order O(n) unless a configuration tool is used (see A2).
