# Task 03 (B3) - Bare metal vs VM vs container, Kubernetes

Labels: [M] measured by me, [C] calculated (working shown), [T] typical/published, [A] assumed.

## Complete the Code: deployment.yaml
 - TODO 1: replicas: 3 and selector.matchLabels app: result-portal.
 - TODO 2: resources requests cpu 250m, memory 256Mi; limits cpu 500m, memory 512Mi.
 - Everything else is unchanged. Applied with kubectl apply -f deployment.yaml.

## B3.3 Kubernetes (kind) [M]
Setup: kubectl client v1.37.2, kind v0.33.0, node image kindest/node:v1.37.0, one-node cluster "portal". The image was tagged simats/result-portal:1.0 and loaded with kind load docker-image.
 - kind has no cloud load balancer, so the required Service (type LoadBalancer) shows EXTERNAL-IP <pending> (CLUSTER-IP 10.96.169.70, 80:31000/TCP). For load tests I added a second, test-only Service result-portal-np (NodePort 30080, mapped to host port 8081 in kind-config.yaml). kube-proxy still spreads requests over the pods.
 - kubectl get pods: 3 pods result-portal-86c9577d7f-* READY 1/1, STATUS Running, on node portal-control-plane.
 - kubectl describe pod: Limits cpu 500m, memory 512Mi; Requests cpu 250m, memory 256Mi; Readiness http-get :8000/health, period 10 s, failureThreshold 3. The events also showed one "Readiness probe failed ... connection refused": the first probe ran before gunicorn was listening. The pod became Ready on a later probe.
 - Delete demo: right after kubectl delete pod the old pod was Terminating and a new pod was already ContainerCreating (age 0s). 8 s later the new pod was Running but READY 0/1; traffic only goes to pods that are Ready.
 - Recovery time (script deletes a Ready pod, waits for 3 other Ready pods; 5 runs): 10.9, 1.9, 11.5, 1.8, 1.8 s. Median 1.9 s. Three runs recovered in about 1.8 to 1.9 s; two runs took about 11 s. The cause is the readiness probe: when its first probe fires before gunicorn is listening, the next probe is 10 s later [C: 10 s period + about 1 s start]. So typical recovery is about 2 s and worst observed 11.5 s. Setting periodSeconds to 2 or using a startupProbe would remove the slow case [T], but I did not test that.
 - My first recovery run (recovery_output_run1.txt: 11.0, 1.8, 1.8, 0.3, 1.8) is kept as evidence. Its run 4 (0.3 s) is invalid: my script chose a pod that was already Terminating. I fixed the script to pick only Ready pods and re-ran it.
 - Scaling 3 to 6 under load (ab -c 50 through the NodePort):
   3 replicas: 1342.66, 1596.14, 1236.74, 1299.65, 1430.49 req/s, median 1342.66.
   Scaled with kubectl scale to 6 while a 200000-request ab run was in progress: the new pods were Ready after about 3 s and the load run finished at 1636.05 req/s with 1 failed request out of 200000 [C: 0.0005 %].
   6 replicas: 729.48, 2277.44, 2669.35, 714.17, 2326.87 req/s, median 2277.44.
   Speed-up = 2277.44 / 1342.66 = 1.70 times [C] (ideal 2.0).
   Caveat: the 6-replica runs split into two groups (two runs near 720, three between 2277 and 2669). I did not find the cause; ab, the port mapping and the 6 pods all share the same 8 CPUs in WSL2, which is a likely reason [A]. I therefore read the result as "scaling clearly helps but is not exactly 2 times".

## B3.1 / B3.2 The same app three ways [M]
The same Task02/app.py with 1 gunicorn worker. Bare metal: gunicorn run directly on the host. VM: KVM/QEMU, Ubuntu 24.04 cloud image, 2 vCPU, 1 GiB RAM, app started by systemd inside it. Container: the Task02 image. ab -n 30000 -c 20 from the host, 5 runs each. The kind cluster was stopped during the test.

 Deployment  | start-up (s), 5 runs, median       | memory                                   | disk / image size                      | requests per second, median
 Bare metal  | 0.56 0.28 0.21 0.19 0.21 -> 0.21   | 62.1 MiB (sum of RSS of 2 gunicorn)      | 22 MiB virtualenv (+ shared OS Python) | 1713.64 1761.49 1530.69 1649.55 1573.48 -> 1649.55
 Container   | 1.00 0.82 0.77 0.68 0.69 -> 0.77   | 38MiB (docker stats)                     | 201MB disk, 49MB compressed (Task02)   | 874.94 970.11 933.03 938.81 928.12 -> 933.03
 KVM VM      | 15.43 14.18 15.51 13.48 13.91 -> 14.18 | 553 MiB (host RSS of QEMU; guest has 1024 MiB) | base image 593M + overlay file 610M    | 1663.33, 793.16, 514.91 valid; 2 runs printed no result -> median of the 3 valid runs = 793.16

Ratios [C]: container / bare start-up = 0.77 / 0.21 = 3.7 times; VM / container start-up = 14.18 / 0.77 = 18.4 times. Throughput relative to bare metal: container 933.03 / 1649.55 = 0.57; VM 793.16 / 1649.55 = 0.48.

Caveats (honest):
 - VM load test: 2 of the 5 ab runs produced no "Requests per second" line, so only 3 runs are valid and the median is of those 3. I did not capture why ab failed. The VM is reached through QEMU user-mode networking (slirp), which adds overhead and is not how a production VM is networked [A]. Treat the VM throughput as a lower bound.
 - The memory numbers measure different things (RSS of processes, cgroup usage, RSS of the QEMU process) and are not an exact like-for-like comparison.
 - Container throughput varies between sessions: in Task02 the same image gave 1295.90 req/s (unlimited, median of 5), now 933.03. So do not read the 0.57 ratio as precise.
 - The VM overlay file grew to 610M after the first boot (package install and filesystem resize); I did not investigate further.

## B3.4 Engineering trade-off
 - Bare metal: fastest start (0.21 s) and highest throughput (1649.55 req/s), but no isolation, the app depends on the host's packages, and it is hard to move or scale. Use it for a single heavy workload that needs all the hardware, or a tiny setup.
 - Container: starts in under a second (0.77 s), uses little memory (38MiB) and a small image (49MB compressed), is identical on every machine, and Kubernetes can replace and scale it (recovery median 1.9 s, 3 to 6 pods under load). It shares the host kernel, so isolation is weaker. Use it for the stateless web tier of the portal.
 - VM: separate kernel and the strongest isolation, snapshots and live migration, can run a different OS; but 14.18 s start-up, 553 MiB of host memory and about 1.2 GB of disk for one small service. Use it for the database, for exam systems that need strong isolation, or for different operating systems.

## Alternative concept: Docker Compose scaling (docker compose up --scale portal=6) instead of Kubernetes
 - What changes: one server runs 6 containers. A fixed host port mapping (8000:8000) cannot be used for 6 copies, so a reverse proxy such as Nginx has to spread requests. There is no autoscaler and no readiness-based routing unless added by hand.
 - Recovery from a failed container: a restart policy (restart: always) restarts it on the same server [T]; I did not measure it. Recovery from a failed SERVER: nothing moves the containers to another machine, so the portal is down. Kubernetes on several nodes reschedules the pods on other nodes [T]; I measured pod recovery only (median 1.9 s), on a single-node kind cluster, so I did not demonstrate node failure.
 - Resources: Compose needs no control plane, so less overhead; Kubernetes adds control-plane components I did not measure.
 - Maximum load: Compose is limited to one machine's CPUs; Kubernetes adds nodes. Throughput of n pods is about n x r until the CPUs are used up. Measured: 3 pods 1342.66, 6 pods 2277.44 req/s on one 8-CPU machine [M]. Compose capacity is O(C) for C cores of one host, Kubernetes is O(N x C) for N nodes [C].
