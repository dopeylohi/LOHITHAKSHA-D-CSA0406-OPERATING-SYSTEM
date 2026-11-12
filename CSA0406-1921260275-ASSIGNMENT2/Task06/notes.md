# Q6 – Deadlock handling and the Banker's algorithm: shared laboratory equipment scheduler (notes)
Labels: [M] measured on my machine, [C] calculated from [M] or from the given data, [T] typical published/general knowledge, [A] assumed for the example.
Machine: Ryzen 5 7235HS, WSL2, gcc -O2; each timing = median of 5 runs.

## A. Characterise deadlock (2 marks)
Four necessary conditions, as they appear in the cluster:
| Condition | In the research cluster |
|---|---|
| Mutual exclusion | A GPU, a software licence seat or a storage channel is used by one job at a time. |
| Hold and wait | A job keeps its GPU while it waits for a licence. |
| No pre-emption | A GPU or licence cannot be taken from a running job without destroying its work. |
| Circular wait | J1 waits for what J2 holds, J2 waits for what J3 holds, J3 waits for what J1 holds. |
Resource-allocation graph (single-instance resources; arrows: job -> resource = request, resource -> job = assignment):
~~~
  J1 --requests--> Licence --held by--> J2 --requests--> Storage --held by--> J3 --requests--> GPU --held by--> J1
~~~
The cycle J1 -> Licence -> J2 -> Storage -> J3 -> GPU -> J1 shows all four conditions at once: every resource is exclusive, each job holds one resource and waits for another that cannot be taken away, and the waits form a cycle. With one instance per resource a cycle means deadlock [T]. (A drawn version of this graph is in the report.)

## C. Banker's algorithm: implementation and results (banker_output.txt) [M]
Implementation: banker.c (TODO 1 completed). One fix outside the TODO: the paper code had "││" (box-drawing characters) in request(), which does not compile; I replaced it with "||".
Output: "initial state safe: P1 P3 P4 P0 P2", "P1 request: granted, safe sequence P1 P3 P4 P0 P2", "P4 request: must wait (not available)", "P0 request: denied (unsafe)" - identical to the expected output.
Hand check [C], total resources (10, 5, 7); Need = Max - Allocation: P0 (7,4,3), P1 (1,2,2), P2 (6,0,0), P3 (0,1,1), P4 (4,3,1); Available (3,3,2).
* Initial state: work (3,3,2): P1 needs (1,2,2) fits -> work (5,3,2); P3 (0,1,1) -> (7,4,3); P4 (4,3,1) -> (7,4,5); P0 (7,4,3) -> (7,5,5); P2 (6,0,0) -> (10,5,7). Safe sequence P1 P3 P4 P0 P2.
* P1 requests (1,0,2): (1,0,2) <= need (1,2,2) and <= available (3,3,2); tentatively available becomes (2,3,0), P1 holds (3,0,2) and needs (0,2,0). Safety check from work (2,3,0) again finds P1 P3 P4 P0 P2: granted.
* P4 requests (3,3,0): within its need (4,3,1), but available is (2,3,0) and 3 > 2 for the first resource, so P4 must wait.
Why P0's request (0,2,0) leads to an unsafe state [C]: it is within P0's need (7,4,3) and within available (2,3,0), so the system pretends to grant it: available becomes (2,1,0), P0 holds (0,3,0) and needs (7,2,3). Now no process can finish with work (2,1,0):
| Process | Need | Fits in (2,1,0)? |
|---|---|---|
| P0 | (7,2,3) | No (7 > 2) |
| P1 | (0,2,0) | No (2 > 1 for the second resource) |
| P2 | (6,0,0) | No (6 > 2) |
| P3 | (0,1,1) | No (1 > 0 for the third resource) |
| P4 | (4,3,1) | No (4 > 2) |
No process can complete, so no safe sequence exists: the state is unsafe and the request is denied (the state is rolled back). An unsafe state is not yet a deadlock, but the system can no longer guarantee that every job can finish.

## D. Prediction (written before measuring)
The safety algorithm needs at most n passes over n processes, each check compares m resources, so the worst case grows as O(m x n^2): 10x more processes should take about 100x longer, and doubling m should roughly double the time. In the best case (every process finishes in the first pass) it grows as O(m x n): 10x more processes should take about 10x longer.

## E. Measurement against the prediction (bench_results.txt) [M]
Test states (banker_bench.c, same algorithm as banker.c): "best" = everything fits at once; "worst" = a chain in which only one process can finish per pass (checked for n = 10: the order is P9 P8 ... P0). m = 3 unless stated.
| Case | n | median time per safety check | growth for 10x more processes [C] |
|---|---|---|---|
| best | 10 | 0.037 us | |
| best | 100 | 0.297 us | 8.0x |
| best | 1,000 | 2.937 us | 9.9x |
| worst | 10 | 0.128 us | |
| worst | 100 | 10.381 us | 81x |
| worst | 1,000 | 900.383 us | 87x |
| worst | 1,000, m = 6 | 1,478.0 us | |
| worst | 1,000, m = 12 | 2,469.0 us | |
* Best case: 8.0x and 9.9x for 10x more processes, matching the predicted 10x (linear in n).
* Worst case: 81x and 87x against the predicted 100x: clearly quadratic (n^2), but about 13 to 19 % below 100x. The chain needs about n(n+1)/2 = 500,500 process checks at n = 1,000, i.e. 1.8 ns per check [C]; the small-n runs carry a larger share of fixed cost (copying work, clearing finish[]), which is the likely reason for the shortfall (not tested separately).
* Resource types: doubling m did not double the time: m 3 -> 6 gave 1.64x and 6 -> 12 gave 1.67x. A straight-line fit time = a + b x m gives about 377 us fixed + 174 us per resource type at n = 1,000 [C]; so the growth is linear in m but not proportional - my prediction "roughly double" was only partly right, because every check also pays fixed loop overhead that does not depend on m.
* Cost of a safety check at n = 1,000, m = 3: 2.9 us (best) to 900 us (worst case), i.e. at most 0.9 ms [M].

## Alternative concept: detection and recovery
Solution: allow every request that resources permit (no check at request time). Every T minutes a detection algorithm (the same loop as the safety algorithm, but using the current requests instead of the maximum demands) looks for jobs that can never finish; if it finds a deadlocked set, the scheduler recovers by aborting one or more jobs (or pre-empting resources from them) and restarts them later.
Costs (assumptions [A]: 10 requests per second, detection every T = 5 minutes, n = 1,000 jobs, m = 3):
| Item | Banker's algorithm on every request | Detection every 5 minutes |
|---|---|---|
| Time per request | 2.9 us (best) to 900 us (worst) [M] | none |
| CPU load, n = 1,000, worst case | 900 us x 10 per second = 0.90 % of one CPU [C] | 900 us per 300 s = 0.0003 % [C] |
| CPU load, n = 100, worst case | 10.4 us x 10 per second = 0.01 % [C] | negligible [C] |
| Complexity | O(m x n^2) for every request [T] | O(m x n^2) once per period [T] |
| Work lost | none: a job that would make the state unsafe is made to wait | an aborted job loses its work since its last checkpoint |
| Idle time | none | deadlocked jobs keep their resources idle for T/2 = 2.5 minutes on average [C] |
Work lost when a job is aborted [A][C]: a job with no checkpoint that has run 2 hours on 1 GPU loses 2 GPU-hours; with a checkpoint every 30 minutes it loses on average 15 minutes = 0.25 GPU-hour. Detection needs no advance knowledge of maximum demands, but it pays in lost work and idle resources; the Banker's algorithm pays CPU time and delays (the example shows P4 waiting and P0 denied although no deadlock has happened yet).

## B. Comparison of the four strategies for the cluster
| Strategy | How it works here | Cost / problem | Verdict for the cluster |
|---|---|---|---|
| Prevention (remove one condition) | Mutual exclusion cannot be removed for GPUs/licences. Hold and wait: a job requests everything at start (all or nothing). Circular wait: a fixed global order (GPU, then licence, then storage). No pre-emption: take resources back (needs checkpoints). | All-or-nothing wastes resources (a job holds idle GPUs for hours) and lowers utilisation [T]; ordering is cheap but forces jobs to ask in a set order; pre-emption of a GPU destroys work. | Ordering is cheap and good as a rule; all-or-nothing is simple but wasteful. |
| Avoidance (Banker's algorithm) | Grant a request only if the system stays in a safe state. | Needs every job's maximum demand in advance; O(m x n^2) per request (0.003 to 0.9 ms at n = 1,000 [M]); conservative: it may delay requests that would not deadlock. | Fits a batch cluster where job scripts declare their needs. |
| Detection and recovery | Let deadlock happen, run detection periodically, abort a job. | No advance information, high utilisation, cheap (0.0003 % CPU [C]); but lost work and idle time (T/2) and an aborted job restarts. | Reasonable fallback; costly for long jobs without checkpoints. |
| Ignore (ostrich) | Do nothing; the administrator restarts the cluster. | Zero cost; but the problem statement says the whole cluster stops until someone intervenes. | Acceptable only if deadlock is extremely rare; not here. |

## F. Final design recommendation (2 marks)
Recommendation: use the Banker's algorithm (avoidance) as the scheduler's allocation rule, with every job script required to declare its maximum GPU, licence and storage demand, plus a simple fall-back for jobs that cannot declare their demand (periodic detection and abort, or wait with a time-out).
Reasons:
* Maximum demands are known at submission time in a batch cluster (the job script already asks for GPUs, licences and storage), which is the condition the Banker's algorithm needs.
* The cost is small: m = 3 resource types, and a safety check takes 10.4 us in the worst case at n = 100 and at most 0.9 ms at n = 1,000 [M], against jobs that run minutes or hours; at 10 requests per second [A] this is about 0.01 % to 0.9 % of one CPU [C].
* It removes the failure described in the problem (the whole cluster stopping) without aborting jobs: no work is lost, jobs are only delayed.
Costs and limits to accept:
* If a job underestimates its maximum, the system either rejects its request or can no longer guarantee safety, so declarations must be checked by the scheduler and must not be exceeded.
* If a job overestimates, utilisation drops, because the algorithm keeps resources for demand that never comes.
* The algorithm is conservative: in the given example it delays P4 and denies P0 although nothing has deadlocked yet.
* The cost grows quadratically with the number of jobs: if the cluster ever has many thousands of concurrent jobs, the check cost (about 0.9 ms at n = 1,000, and about 90 ms at n = 10,000 if it keeps scaling with n^2 [C]) would have to be reconsidered, e.g. by running the check only for the affected resource classes.
* A global resource order (GPU, then licence, then storage) is a cheap extra rule and can be added on top.
Limits of the evidence: the benchmark uses constructed best and worst states with m = 3, 6 and 12, not a real cluster trace; request rate, detection period and checkpoint interval are assumptions [A].
