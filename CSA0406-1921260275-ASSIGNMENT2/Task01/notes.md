# Q1 – Processes and Process States (notes)
Labels: [M] measured on my machine, [C] calculated from [M], [T] typical published/general knowledge.
Machine: AMD Ryzen 5 7235HS, 8 logical CPUs, 8.7 GiB RAM, Ubuntu 26.04.1 on WSL2, gcc 15.2.0.

## 1. Process-state diagram
      admit                 scheduler dispatch
NEW ------------> READY ------------------------> RUNNING ----exit----> TERMINATED
^ <---- interrupt / quantum expiry ---| |
| |
+----- I/O or event completes ---- WAITING <-- I/O or event wait
Observed with fork_demo: child 0 = R (running/ready), child 1 = S (waiting, sleep), child 2 = Z (terminated, PCB kept until parent calls waitpid) [M].

## 2. Process control block (PCB)
| PCB field | Meaning | Where seen on Linux |
|---|---|---|
| Process id, parent id | identity | Pid / PPid in /proc/<pid>/status |
| Process state | new/ready/running/waiting/terminated | State: R, S, Z |
| Program counter, CPU registers | saved on context switch | kernel task_struct (not shown) |
| Scheduling information | priority, run-queue data, switch counts | voluntary_/nonvoluntary_ctxt_switches |
| Memory-management information | page tables, limits | VmRSS |
| Open files / I/O status | file-descriptor table | /proc/<pid>/fd |
| Accounting | CPU time used | ps, /proc/<pid>/stat |

## 3. fork() evidence [M]
fork_demo_output.txt: child 0 State R (nonvoluntary switches 2, voluntary 0), child 1 State S (WCHAN hrtimer_nanosleep, voluntary 1), child 2 State Z; after waitpid /proc/<pid> no longer exists (PCB freed).

## 4. Evaluation: where does time go with 2,000 sessions? (bench_results_v2.txt, medians of 5)
| Measure | Process | Thread |
|---|---|---|
| Creation per task [M] | 137.7 us | 78.8 us |
| Creating all 2,000 [M] | 275.3 ms | 157.5 ms |
| Memory per idle task [M] | 116.0 kB | 30.0 kB (64 kB stack set) |
| Memory for 2,000 [C] | 232,000 kB (~227 MiB) | 60,000 kB (~59 MiB) |
| Context switch, round trip [M] | 6.48 us | 5.80 us |
Assumptions: tasks are idle (blocked); real sessions with private heaps will use far more memory [not measured]. The ping-pong time includes 4 pipe system calls, so a pure switch is cheaper than the figures shown.
* Context switch [C]: overhead = switches/s x 3.24 us (process figure, upper bound): 1,000/s = 0.3 %, 10,000/s = 3.2 %, 100,000/s = 32 % of a CPU.
* Memory [C]: processes cost 172,000 kB (~168 MiB) more than threads for 2,000 idle sessions. Small on an 8 GB machine; not enough alone to explain hanging.
* I/O waiting: most student sessions sit in state S waiting for the network or disk, so they use memory but little CPU (state S seen in fork_demo). Time spent waiting was not measured.
* Honest conclusion: the process-vs-thread overhead I measured is modest (about 1.7x creation, 3.9x memory, about 10 % switch). The hanging is more likely caused by a shared bottleneck (CPU run queue, disk, database, memory under real per-session data) – this is a hypothesis, not measured here.

## 5. Design decision
Options: (a) one process per student, (b) one thread per student, (c) fixed pool of worker processes.
Decision: **(c) a fixed pool of worker processes (about 2 x cores = 16 here), each serving many sessions with threads or an event loop**, with separate pools for auto-save and for proctoring video.
Why: (1) it keeps memory and creation cost bounded, whatever the number of students (2,000 sessions x 116 kB is cheap, but real per-session memory is not); (2) a crash kills only one worker, not the whole server (threads in one process would all die); (3) a bounded pool stops overload from creating thousands of runnable tasks; (4) the heavy video work is isolated from the quick answer-saving work.
Trade-off: sessions in one worker share its fate and a worker needs session-routing logic.

## 6. Alternative concept: thread per session in one server process
server(): for each login: pthread_create(session_thread, session)
session_thread(s): loop { read request; process; write reply } until logout
* Time: creating n sessions is O(n) for both designs; constant 78.8 us vs 137.7 us [M].
* Space: O(n x s); s = 30 kB (thread) vs 116 kB (process) [M]; threads share one address space.
* Switch: threads about 10 % cheaper [M]; no address-space switch.
* Crash: a bad pointer in one session thread kills every session (single address space); a crashed process kills only itself.
* Sharing: threads share data through memory (needs locks); processes need IPC.
