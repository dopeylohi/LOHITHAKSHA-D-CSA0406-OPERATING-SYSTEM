# Task07 - Memory-Management Plan for a Smart-Factory Edge Device (10 marks)

Setup: Ubuntu 26.04.1 on WSL2, gcc 15.2.0, `-std=c11 -Wall -Wextra -O2` (build.sh). Labels: [M] measured, [C] calculated, [T] textbook, [A] assumed.
The device has 2 GB RAM (2048 MB) and no swap disk (given).

## Stage 1 - Interpret the problem (2 marks)
Table 7.1 - tasks:
| Task | Memory need | Timing constraint | Tolerates page faults? |
|---|---|---|---|
| Real-time control | 16 MB working set [A] | must never be delayed by a page fault (given); assumed period 50 ms and deadline 10 ms [A] | No |
| Machine learning | 1.5 GB = 1536 MB (given) | soft: results should arrive in reasonable time, no hard deadline [A] | Yes, it only gets slower |
| Logging | 64 MB buffer ring [A] | none hard, may lag behind | Yes |
| OS, kernel, libraries | 300 MB [A] | - | - |
- [C] Memory left for the three tasks: 2048 - 300 = 1748 MB. They need 16 + 1536 + 64 = 1616 MB, so 132 MB (6.4% of RAM) is spare.
- Key constraint: there is no swap. Heap and stack pages cannot be written to disk, so running out of RAM does not only slow a process down, the kernel kills one (out-of-memory killer, as seen in Task04). Only clean file-backed pages (program code, a model file mapped read-only, files) can be dropped and read again from flash. The design therefore assumes that the ML model is mapped read-only from a file [A].
Measurable requirements - Table 7.2:
| ID | Requirement | Result |
|---|---|---|
| R1 | The real-time task has no major page faults and no page-fault delay in its loop | locked runs: 0 major faults, 1 minor fault per 10 s run [M] |
| R2 | Real-time response (release to completion) under 10 ms [A] | locked, worst of all 15 runs: 1.593 ms [M]; margin 10 / 1.593 = 6.3 times [C] |
| R3 | Memory committed by all tasks at most 1748 MB | 1616 MB used, 132 MB spare [C] |
| R4 | ML task has no steady-state page faults while the three tasks run | 0 faults [M, simulation S1] |
| R5 | A new task must fit in the spare 132 MB, otherwise the design is changed first | applied in Stage 4 |
| R6 | Logging has no deadline but cannot take memory from the others: its ring is fixed at 64 MB | by design |

## Stage 2 - Compare candidate mechanisms (3 marks)
Table 7.3 - comparison and justification:
| Task | Contiguous allocation | Paging | Segmentation | Demand paging | Decision |
|---|---|---|---|---|---|
| Real-time control | predictable and simple, but needs one contiguous block and fragments on a device that runs for years | yes, no external fragmentation, bounded translation through the TLB | no extra benefit; Linux uses flat segments | NOT acceptable unless the pages are locked: a fault delayed the loop by about 20 ms in my test | paging, all pages locked with mlockall |
| Machine learning (1.5 GB) | needs a 1.5 GB hole; fails after fragmentation | yes, pages can be anywhere | no benefit, would add external fragmentation | yes: model file mapped read-only, loaded on use, clean pages can be dropped without swap; but only while the working set fits (Stage 3: 0 faults with 3 tasks, 1536 per sweep with 4) | paging with demand paging |
| Logging | a growing log needs a growing block: poor | yes | no | yes: pages are created when written and written back to flash | paging with demand paging |
Locking method: the real-time task calls mlockall(MCL_CURRENT | MCL_FUTURE) once at start-up, after it has allocated and touched (pre-faulted) all its buffers and before its loop; it does not allocate memory inside the loop. Locked pages are never swapped or reclaimed. It needs the CAP_IPC_LOCK capability or a large RLIMIT_MEMLOCK (I ran it with sudo). The 16 MB locked is only 0.9% of the 1748 MB available [C]. I do not lock the ML and logging tasks: they tolerate faults, and locking 1.6 GB would remove the OS's freedom to drop pages.

## Stage 3 - Implement and verify (3 marks)
### 3a. Locking program and worst-case delay (rt_loop.c, rt_test.sh) - Table 7.4
Method [M]: a periodic loop with period 50 ms; each cycle it touches every page of a 32 MB buffer (8192 pages of 4 KB; the test buffer is twice the assumed 16 MB), response time = finish time minus scheduled release time; 10 s per run (199 cycles); 5 runs per condition, run as root. Lock = mlockall(MCL_CURRENT | MCL_FUTURE). Three conditions: none (RAM plentiful); pageout = after each cycle the program calls madvise(MADV_PAGEOUT), asking the kernel to reclaim the buffer while the task sleeps (forced, simulated memory pressure [A]); hog = a second process keeps touching 300 MB in the same cgroup with MemoryHigh 200 MB (real memory pressure). Results from rt_out.txt, median over 5 runs unless stated:
| Condition | Lock | Median of worst case (ms) | Worst of 5 runs (ms) | Median p99 (ms) | Median mean (ms) | Deadline misses (>10 ms), 5 runs | Cycles per run | Minor / major faults |
|---|---|---|---|---|---|---|---|---|
| none | no | 0.865 | 1.143 | 0.717 | 0.334 | 0 | 199 | 2 / 0 |
| none | yes | 0.809 | 1.593 | 0.545 | 0.348 | 0 | 199 | 1 / 0 |
| pageout | no | 20.155 | 26.605 | 18.480 | 14.775 | 232 | 93, 50, 43, 50, 49 | 248,413 to 753,168 / 0 |
| pageout | yes | 1.059 | 1.337 | 0.813 | 0.360 | 0 | 199 | 1 / 0 |
| hog | no | 1.441 | 2.710 | 0.932 | 0.318 | 0 | 199 | 2 (once 1337) / 0 |
| hog | yes | 0.576 | 0.655 | 0.465 | 0.300 | 0 | 199 | 1 / 0 |
Raw worst-case values (ms) per run [M]: none/no 1.143, 0.761, 0.552, 0.865, 1.032; none/yes 0.809, 0.678, 0.994, 0.616, 1.593; pageout/no 20.155, 19.856, 26.605, 19.947, 21.664; pageout/yes 0.725, 1.059, 0.909, 1.337, 1.108; hog/no 1.403, 2.710, 1.706, 1.280, 1.441; hog/yes 0.655, 0.576, 0.454, 0.449, 0.622.
Working [C]:
- Forced reclaim: the median worst case fell from 20.155 ms to 1.059 ms with locking, 20.155 / 1.059 = 19.0 times lower. Without locking the task missed the 10 ms deadline in 232 of 285 completed cycles (92 + 49 + 27 + 23 + 41 = 232; 93 + 50 + 43 + 50 + 49 = 285), 81%; with locking 0 of 995 cycles (5 x 199).
- Every unlocked cycle had to fault most of its pages back in: 753,168 / 93 = 8,099 minor faults per cycle in run 1, and 5,104 in the lowest run (255,221 / 50); the buffer has 8192 pages, so 62% to 99% of its pages. Unlocked runs finished only 43 to 93 cycles out of 199, because late cycles made the following releases be skipped (93 to 143 releases skipped per run).
- Cost per fault: 15.148 ms mean / 8,099 faults = about 1.9 microseconds. That is cheap because the kernel counted all of them as minor faults (0 major), so I believe the pages were still in memory (swap cache) and no disk read happened (my guess, not verified). A real disk read would make every fault far more expensive, so this 20 ms is probably the mild case.
- Plentiful RAM (none): locked and unlocked are the same within noise (median worst case 0.809 against 0.865 ms; the single largest value, 1.593 ms, was a locked run). Locking costs nothing visible and gains nothing when memory is not short, as expected [T].
- Real hog: median worst case 1.441 ms unlocked against 0.576 ms locked, 2.5 times lower, but all hog runs had 0 major faults and 0 deadline misses, and the unlocked task took only one burst of 1,337 minor faults (worst case 2.710 ms). So the real hog hardly disturbed the task: the task touched its pages every 50 ms, so the kernel treated them as recently used and evicted the hog's pages instead. I do not claim the hog test proves the benefit; the evidence for it is the forced-reclaim test.
- With locking, over all 15 locked runs the worst case was 1.593 ms, 6.3 times below the 10 ms deadline. The remaining delay cannot come from page faults of the buffer (1 minor and 0 major fault per run); it is probably scheduler and WSL noise (not tested).
### 3b. Page-replacement simulation of the three tasks (simulate.c) - Table 7.5
Simulator [A]: global LRU replacement, 1 simulated page = 1 MB (256 real pages), RAM 2048 MB, OS reserve 300 MB, 30,000 ticks of which the first 3,000 are warm-up (not counted). Each tick the ML task reads 8 consecutive model pages (cyclic sweep over 1536), the logging task writes the next page of its 64 page ring, and, if present, the fourth task reads 4 random pages of its 500; the real-time task runs every 300 ticks and uses its 16 pages. Locked pages sit outside the replacement pool. Results [M] (simulate_out.txt):
| Scenario | Demand (MB) | Frames for unpinned pages | RT faults | ML faults | Logging faults | Task 4 faults |
|---|---|---|---|---|---|---|
| S1: 3 tasks, RT locked | 1600 | 1732 | 0 | 0 | 0 | - |
| S2: 3 tasks, RT not locked | 1616 | 1748 | 0 | 0 | 0 | - |
| S3: 4 tasks, RT locked | 2100 | 1732 | 0 | 216,000 (1536 per sweep) | 0 | 29,378 |
| S4: 4 tasks, RT not locked | 2116 | 1748 | 1,440 (16 in every activation) | 216,000 (1536 per sweep) | 0 | 29,203 |
Verification that the real-time task is never delayed [M, C]:
- Locked (S1 and S3): 0 page faults for the real-time task, even when memory is short (S3).
- S2: 0 faults although it is not locked, because 1616 MB fit into 1748 MB and nothing is ever evicted: locking only matters when memory is short.
- S4: not locked and short of memory, the task faulted on every one of its pages at every activation: 1,440 = 90 activations x 16 pages. This is the delay the lock prevents.
- Consistent with the measurement of 3a: locking removes the delay when pages are reclaimed, and makes no difference when there is enough RAM.
- The ML task's 216,000 faults are 27,000 ticks x 8 pages = every single ML access: a cyclic sweep over more pages than there are frames is the worst case for LRU. This is the result of the model, it is not a measured speed.

## Stage 4 - Defend the design (2 marks)
Justification with measurements:
1. Without a lock, pages reclaimed by the kernel pushed the median worst case to 20.155 ms and caused 232 deadline misses; with mlockall it was 1.059 ms and 0 misses (Table 7.4).
2. The simulation shows that the lock keeps the real-time faults at 0 in the scenario where memory is short (S3) while S4 gives 1,440.
3. The three given tasks fit: 1616 of 1748 MB, 132 MB spare (Table 7.2), so the ML task needs no replacement (0 faults in S1).
Effect of a fourth task of 500 MB [C]: demand rises to 1616 + 500 = 2116 MB against 1748 MB available (2100 against 1732 with the 16 MB locked), 368 MB too much. In the simulation the ML task goes from 0 to 216,000 faults (every access) and the fourth task adds 29,378 faults, while the locked real-time task stays at 0 (S3). The simulation can not show the real danger: with no swap the pages of a 500 MB heap cannot be evicted, so the kernel would kill a process (out-of-memory killer, seen in Task04); I did not measure this for the device. What I would do, in this order:
- Admission control: accept a new task only if its memory fits into the spare 132 MB (R5); the 500 MB task is refused as it is.
- Give each non-real-time task a cgroup memory limit (memory.max) so a task cannot take memory from the others, and keep the real-time task locked.
- Reduce the ML footprint (a smaller or quantised model; the gain is not measured) or add RAM, if the fourth task is needed.
- Optionally add swap on flash for the non-real-time tasks only: it avoids the out-of-memory kill but adds flash wear and slow faults; the real-time task stays locked.
One remaining limitation: mlock removes only page-fault delay. Even locked, my loop had a worst case of 1.593 ms on a non-real-time kernel (WSL), so other delays (scheduler, interrupts, cache and TLB misses) remain; a hard deadline would also need a real-time kernel (PREEMPT_RT), a real-time scheduling class (SCHED_FIFO) and an isolated CPU [T]. I did not test these.

## Alternative: real-time operating system with no virtual memory instead of Linux with locked memory
- Worst-case delay of the control task: with no virtual memory there are no page faults at all, and an RTOS publishes a bounded worst-case interrupt and scheduling latency (typically tens of microseconds on small devices [T]). My Linux result is 1.593 ms worst case on a non-real-time kernel [M]; the two numbers come from different machines, so I do not state a ratio. The RTOS delay is bounded by design, Linux's is only measured.
- Memory for the ML task [C, A]: Linux leaves 2048 - 300 - 16 - 64 = 1668 MB for the ML task and spare memory. An RTOS with an assumed 50 MB footprint leaves 2048 - 50 - 16 - 64 = 1918 MB, 250 MB more than Linux (1918 - 1668); the ML task needs 1536 MB, so 382 MB would be spare. But all memory is allocated statically and fully resident: no demand paging, no mapping the model from a file, so a task that does not fit (the 500 MB task: 500 > 382) is refused when it is loaded instead of slowing the others down.
- Effort of adding tasks: higher with an RTOS. Every task needs a fixed memory budget in advance, tasks are usually compiled and linked into the image, and ML libraries and POSIX tools may not exist for the RTOS. With Linux a new task is a new process with a cgroup limit and a lock decision, but its memory use can hurt the other tasks unless limited (Stage 4).
- Complexity [T]: translation in a design without virtual memory is O(1) with no table walk; Linux with locked memory is also O(1) per access once the page is mapped, but a TLB miss walks 4 levels on x86-64. My simulator costs O(n x P) for n references and P pages in the pool, because it scans for the LRU victim at every fault.

## Limits
- The real hog did not create enough pressure to evict the task's pages (0 major faults in all runs); the 20 ms result is from forced reclaim by madvise, a simulation of memory pressure.
- Task sizes, period, deadline and OS reserve are assumed; the simulator is a model at 1 MB granularity, and all its numbers are model results, not measurements of a device.
- One machine (WSL2, 5 runs per condition); the locked worst case of 1.593 ms comes from a non-real-time kernel.
