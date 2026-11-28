# CSA0406 Operating Systems - Assignment 5 (CO5)
Student: Lohithaksha D, Reg No 1921260275, Information Technology, MS2 Slot B, 1st Year 2nd Sem
Faculty: Jayasakthi Velmurugan Kannaiyan, SIMATS Engineering

## Folder guide
- TaskA1-TaskA5: Part A design notes (notes.md in each).
- Task01-Task04: Part B implementation (B1 server, B2 containers, B3 kind and VM comparison, B4 hybrid plan).
- Each folder has its scripts, outputs and notes.md.

## Environment
Ubuntu 26.04.1 on WSL2, kernel 6.18.58.1. Python 3, Flask 3.1.3, gunicorn 27.0.1, Nginx 1.28.3, Docker with Compose v2, kind v0.33.0, kubectl v1.37.2, QEMU/KVM, apache2-utils (ab). No C code was needed.

## Run
Run each folder's scripts, for example `bash Task02/b2_compose.sh`, `bash Task03/b3_k8s_recovery.sh`, `bash Task04/b4_run.sh`, `python3 Task04/b4_capacity.py`. Raw outputs are saved next to the scripts.

## Key results (median of 5 runs unless stated)
- [M] Start-up: bare 0.21 s, container 0.77 s, VM 14.18 s.
- [M] Throughput: bare 1649.55, container 933.03, VM 793.16 req/s (VM: 3 valid runs of 5).
- [M] Memory: bare 62.1 MiB, container 38 MiB, VM 553 MiB.
- [M] CPU limit 0.5: 1295.90 down to 726.46 req/s. OOM kill exit code 137.
- [M] Images: slim 201 MB, multi-stage 188 MB, ubuntu 214 MB.
- [M] Kubernetes: pod recovery median 1.9 s; scaling 3 to 6 replicas 1342.66 to 2277.44 req/s (1.70x).
- [C/A] Capacity and cost for B4; decision is hybrid.

## Labels
[M] measured, [C] calculated, [T] taken from a tool or doc, [A] assumed.

## Deviations and limits
- Existing repo used, with this folder inside it.
- Docker builds use --network=host because of a WSL DNS failure.
- VM test had 3 valid runs of 5 because of slirp networking.
- No HTTPS/TLS was built; SSH password login was not disabled.
- Kubernetes readiness delays gave two recoveries of about 11 s; 6-replica runs were uneven.
- The lms service in B4 is an nginx stand-in.
- Screenshots are in the Word report, not in this repo.
