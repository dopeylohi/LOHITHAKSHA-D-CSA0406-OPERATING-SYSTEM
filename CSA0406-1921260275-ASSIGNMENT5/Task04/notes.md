# Task 4 (B4) - Hybrid cloud and virtualisation plan for the result portal

Labels: [M] measured here, [C] calculated from [M], [A] assumption.

## B4.1 Containers with resource limits (docker-compose.yml, b4_run.sh)
| Service | Image | CPU limit | Memory limit |
|---|---|---|---|
| portal | Flask app (gunicorn) | 0.5 | 256 MiB |
| lms | nginx:alpine (stand-in for the LMS) | 0.25 | 128 MiB |
| db | postgres | 0.5 | 256 MiB |
Output: b4_run_output.txt (docker stats shows all three rows) [M].
Limits are enforced by cgroups. In B2 a limit of 0.5 CPU cut throughput from 1295.90 to 726.46 req/s [M].

## B4.2 Capacity (b4_capacity.py)
Measured: one portal container sustains about 933 req/s [M] (B1 container median).
Assumed [A]: 30,000 students; result day: 25% of students (7,500) log in over 6 hours, average 10 requests per visit, peak = 10x average.
- Average load = 208.3 req/s [C]; peak load = 2083.3 req/s [C].
- Demo scale: 5 containers or 4 VMs [C].
- Realistic scale (x20 factor, [A]): 82 containers or 76 VMs [C].
Containers are about 20 times cheaper to run than VMs for the same load, because VM RAM overhead is 553 MiB against 38 MiB [M].

## B4.3 Decision: hybrid
Yearly cost [A, illustrative prices]: own servers only $7,600; cloud only $3,516; hybrid $1,812.
Decision: hybrid. Keep the database and student data on-premises (control, privacy). Run the portal in containers on the own cluster and burst to cloud on result day.
Why it fits: the load has a short, large peak, so paying for cloud only on peak days is cheaper than owning hardware for the peak.

## Alternative concept: cloud-only (serverless/autoscaling)
Pseudocode: every 30 s, read req/s; desired = ceil(rps / 900); scale to desired within [2, 100].
Complexity: O(1) per decision; scaling cost is linear, O(n) in the number of instances started.
Rejected because the data would leave campus control and the cost-model gap is small.

## Limits and honesty
- All costs and user numbers are [A]; only throughput and memory are [M].
- lms is an nginx stand-in, not a real LMS.
- The x20 factor is an assumption, not a measurement.
