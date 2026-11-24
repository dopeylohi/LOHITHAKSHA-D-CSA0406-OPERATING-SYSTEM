# Task 02 (B2) - Containerise the portal

Labels: [M] measured on my machine, [C] calculated, [T] typical/published meaning, [A] assumed.
Machine: Ubuntu on WSL2, Docker with the classic (legacy) builder, 8 CPUs, 8.7 GiB RAM.

## Complete the Code (Dockerfile)
 - TODO 1: WORKDIR /app
 - TODO 2: RUN useradd --system --create-home --shell /usr/sbin/nologin portal, then COPY --chown=portal:portal app.py . and USER portal
 - TODO 3: EXPOSE 8000 and CMD ["gunicorn", "--bind", "0.0.0.0:8000", "app:app"]
 - The rest of the file is unchanged. requirements.txt is copied and installed BEFORE app.py so the pip layer stays cached when only the code changes.

## Problem I hit: DNS inside the build [M]
 - The first docker build failed in the pip step: "Temporary failure in name resolution" (saved in build_failed_dns.txt). Build containers could not use WSL's DNS.
 - Workaround: docker build --network=host, which lets the build use the host network. I used it for every build, and in the variants script and the Compose file (build: network: host). I did not change the Docker daemon config.
 - The result: "Successfully built 5a6ca19aa44c", "Successfully tagged result-portal:latest" [M].

## Run [M]
 - docker run -d --name portal -p 8000:8000 result-portal: docker ps showed STATUS "Up 3 seconds" and PORTS 0.0.0.0:8000->8000/tcp.
 - curl / gave "University Result Portal (demo)". curl /health gave {"host":"1a74dd642550","status":"ok"}. The host name is the container ID, because the container has its own hostname (its own UTS namespace).
 - docker exec portal whoami printed: portal (not root).
 - docker images: result-portal:latest, ID 5a6ca19aa44c, DISK USAGE 201MB, CONTENT SIZE 49MB. [T] Disk usage is the uncompressed size on disk; content size is the compressed size that is transferred when pulling.

## Image layers (docker history) [M]
 - Base image layers: Debian rootfs 87.7MB, Python build layer 41.4MB, apt layer 4.93MB, small layer 16.4kB. Sum = 134.0MB [C: 87.7 + 41.4 + 4.93 + 0.016].
 - My layers: pip install 17.8MB, useradd 61.4kB, COPY app.py 12.3kB, COPY requirements.txt 12.3kB, WORKDIR 8.19kB. USER, EXPOSE and CMD are 0B (metadata only).
 - What this shows: the base image is most of the size and my application code is about 12 kB. Each Dockerfile instruction is one read-only layer; layers are cached and shared between images.

## Docker Compose with a volume [M]
 - docker-compose.yml has two services: db (postgres:16-alpine) with the named volume dbdata mounted at /var/lib/postgresql/data, and portal (built from the Dockerfile) with DB_HOST=db.
 - Compose created a network task02_default and a volume task02_dbdata. docker compose ps: both containers Up.
 - /dbcheck returned {"db":"db","reachable":true}: the portal container reached the database by the service name "db" (Compose provides DNS on its network). The portal only checks the connection; it does not run queries.
 - I created a table and inserted a row (CREATE TABLE, INSERT 0 1), ran docker compose down (containers and network removed), and docker volume ls still listed task02_dbdata. After docker compose up, SELECT returned the row: 1921260275 | 64 (1 row).
 - What this shows: containers are disposable but a volume keeps the data outside the container's layer.

## CPU and memory limits [M]
Load: ab -n 30000 -c 20 on /result/1921260275, 5 runs per case, median reported. Failed requests were 0 in every run.
 - Unlimited (NanoCpus=0, Memory=0): 1297.01, 1264.05, 1270.45, 1375.10, 1295.90 req/s. Median 1295.90 req/s. docker stats during load: CPU 78.87 %, memory 40.38MiB / 8.73GiB, 3 PIDs.
 - Limited (--cpus 0.5 --memory 128m; docker inspect NanoCpus=500000000, Memory=134217728): 792.55, 668.96, 662.51, 726.46, 853.13 req/s. Median 726.46 req/s. docker stats: CPU 49.69 %, memory 40.57MiB / 128MiB (31.70 %).
 - Throughput with the limit = 726.46 / 1295.90 = 0.56, so 56 % of unlimited [C].
 - Reading: the CPU limit works as a cap. The container was held at 49.69 % of one core (limit 50 %) while unlimited it used 78.87 %. If throughput scaled with CPU, the cap would predict about 0.5 / 0.7887 = 63 % [C]; I measured 56 %. The limited runs vary a lot (663 to 853), and ab runs on the same machine, so I treat this as "about 55 to 65 %", not an exact figure.
 - The memory limit was not reached by the portal (40.57MiB of 128MiB), so the 128m run only shows the limit being applied, not enforced. The next test shows enforcement.

## Memory-limit breach test [M]
 - docker run --memory 20m --memory-swap 20m python:3.12-slim, trying to allocate 200 MiB: no "allocated" line was printed and the exit code was 137.
 - [T] 137 = 128 + 9, i.e. the process was killed with SIGKILL by the kernel's out-of-memory killer. The container's memory cgroup limit is enforced by the kernel, so one container cannot take the whole server's RAM.

## Alternative concept: full Ubuntu base or multi-stage build instead of python:3.12-slim
Files: Dockerfile (slim, required), Dockerfile.multistage (builder stage + final slim stage, only the installed packages are copied), Dockerfile.ubuntu (ubuntu:24.04 + apt python3 + venv). All built with b2_variants.sh using --no-cache, 5 builds each, median [M].

 Variant      | build times (s)                  | median | disk usage | content size | dpkg packages
 slim         | 13.2 12.6 12.8 12.5 13.0         | 12.8 s | 201MB      | 49MB         | 87
 multistage   | 12.8 13.5 14.5 13.7 14.0         | 13.7 s | 188MB      | 45.6MB       | 87
 ubuntu       | 33.9 31.1 30.2 34.8 31.9         | 31.9 s | 214MB      | 52.6MB       | 112

Calculations [C]:
 - multistage vs slim: 201 - 188 = 13MB less on disk (13 / 201 = 6.5 %); content 49 - 45.6 = 3.4MB less (6.9 %).
 - ubuntu vs slim: 214 - 201 = 13MB more on disk (+6.5 %); packages 112 - 87 = 25 more (25 / 87 = +28.7 %); build time 31.9 / 12.8 = 2.5 times slower.
 - Pull time = content size / bandwidth. With an assumed [A] 100 Mbit/s = 12.5 MB/s: slim 49 / 12.5 = 3.9 s, multistage 45.6 / 12.5 = 3.6 s, ubuntu 52.6 / 12.5 = 4.2 s. The difference is under 1 second.

Interpretation:
 - Surprising result: the Ubuntu image is only about 13MB bigger than slim, not hundreds of MB. python:3.12-slim already carries its own built Python (a 41.4MB layer in docker history), while the Ubuntu variant uses the distro's python3. I did not inspect which Ubuntu layers differ, so this reason is an [A] assumption.
 - The clear differences are build time (the Ubuntu build runs apt-get update and installs, 2.5 times slower) and package count (112 vs 87). More installed packages means more software that can contain security problems. I did not run a vulnerability scanner, so I report the package count only, not a number of vulnerabilities.
 - Multi-stage saved 13MB (6.5 %), which is small here. [T] Multi-stage pays off when the build needs compilers or build tools that must not ship in the final image; my dependencies are pure Python wheels, so there was little to throw away.
 - Choice: python:3.12-slim (or multi-stage for a slightly smaller image) because it builds fastest and has the fewest packages.

Complexity (n = number of servers, S = image size, B = bandwidth) [C]:
 - Pull time per server T = S / B, so O(S). Pulling to n servers moves O(n * S) data in total (less with a registry mirror).
 - A rebuild after changing only app.py reuses the cached layers and rebuilds from the COPY app.py step on, so the changed data is about 12.3kB, versus O(S) for a build from scratch.
 - Space: k images that share a base layer store it once on a host, about O(S_base + sum of own layers), not O(k * S).

## What each B2 step shows about containers
 - docker run: a container is just a process (gunicorn) started from an image with its own filesystem, hostname and network port mapping.
 - whoami = portal: the app runs as a non-root user inside the container, which limits damage if it is compromised.
 - docker history: an image is a stack of read-only layers; the base is most of the size, my code is about 12 kB.
 - Compose and volume: services find each other by name; data in a volume survives removing the containers.
 - --cpus and --memory: the kernel (cgroups) caps CPU and memory per container; docker stats shows usage against the limit, and exceeding the memory limit gets the process killed.
