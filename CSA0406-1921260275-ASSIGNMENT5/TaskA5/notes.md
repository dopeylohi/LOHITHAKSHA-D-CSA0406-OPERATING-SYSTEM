# A5 - Cloud deployment and orchestration (4 marks)
Labels: [M] measured by me in Part B, [C] calculated, [T] standard/published fact, [A] assumption.

## A5.1 Cloud set-up (2)
 Model | Who manages what                                   | Example for the portal
 IaaS  | provider: hardware; me: OS, runtime, app, patches  | rent virtual machines
 PaaS  | provider: OS and runtime; me: app and data         | managed Kubernetes or a managed web platform
 SaaS  | provider: everything                               | a ready-made result-publishing product
Design for result day:
 - Managed Kubernetes cluster (PaaS-like) running the portal pods with resource requests and limits and a horizontal pod autoscaler, in at least two availability zones.
 - A load balancer in front, with TLS, spreading requests over the pods.
 - A managed database (backups, failover and read replicas handled by the provider) in a private subnet.
 - A cache or CDN for static pages so most requests never reach the application.
 - Extra capacity is pre-scaled before the published result time and scaled down afterwards.
 Reason: I measured that pods start fast and scale under load [M], while a VM needs about 14 s per instance and more memory.

## A5.2 Kubernetes (2)
 - Pod: smallest unit, one or more containers sharing a network address. Deployment: declares the desired number of replicas and rolls out updates. ReplicaSet (made by the Deployment) keeps that number alive. Service: a stable address that spreads traffic over the pods that are Ready. Replicas: identical copies of the pod.
 - Requests and limits: the request is what the scheduler reserves for the pod; the limit is the cap enforced by cgroups. [M] I set requests 250m CPU and 256Mi, limits 500m CPU and 512Mi, and kubectl describe confirmed them.
 - Readiness probe: a pod gets traffic only after /health answers [M: a new pod showed READY 0/1 until its probe succeeded].
 - Horizontal pod autoscaler: adds or removes replicas to keep average CPU use near a target. Rule [T]: desired = ceil(current replicas x current metric / target metric); for example 3 pods at 90 % with a target of 60 % gives ceil(3 x 90 / 60) = 5 pods [C]. I did not run the autoscaler (it needs a metrics server); I scaled manually.
 - Recovery: if a pod dies, the ReplicaSet creates a new one. [M] Median recovery 1.9 s (5 runs, worst 11.5 s because of the 10 s probe period). Scaling: [M] from 3 to 6 pods under load, new pods Ready after about 3 s, 1 failed request out of 200000, throughput median 1342.66 to 2277.44 req/s (1.70 times).

## Alternative concept: fixed virtual machines sized for result day instead of Kubernetes with an autoscaler
Using my capacity estimate (Task04, realistic case: 41 CPUs at peak [C]) and an assumed price of $0.05 per vCPU-hour [A]:
 - Fixed VMs: 41 vCPU x 8760 h x $0.05 = $17,958 per year [C].
 - Autoscaled: baseline always on, plus a burst of 41 vCPU x 3 h x 2 result days x $0.05 = $12.30 per year [C].
 - Idle resources: the peak lasts about 2 x 3 = 6 hours of 8760, so 6 / 8760 = 0.07 % [C]; the fixed VMs are idle for 99.9 % of the year, including the other 364 days.
 - Response time on result day: fixed VMs respond well as long as my load estimate was right, but cannot grow if it was too low (a new VM needs about 14 s to boot [M] plus provisioning time). The autoscaler adds pods within seconds [M], as long as the cluster has spare nodes.
 - Complexity: fixed capacity costs O(peak x 365 days); autoscaled cost is O(average load over time).
 The prices are assumptions [A], so the ratio is the point (about 1,460 to 1), not the exact dollars.
