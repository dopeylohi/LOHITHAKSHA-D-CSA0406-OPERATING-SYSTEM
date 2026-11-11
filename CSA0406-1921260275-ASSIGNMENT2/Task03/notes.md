# Q3 – Threads: traffic-camera number-plate reader (notes)
Labels: [M] measured on my machine, [C] calculated from [M], [T] typical published/general knowledge.
Machine: AMD Ryzen 5 7235HS, 4 cores x 2 hardware threads = 8 logical CPUs, WSL2. Compiled with gcc -O2 -pthread.

## 1. Program
64 cameras, 80 synthetic 320x240 grey frames per camera. Work per frame: generate the frame, 3x3 blur, count high-contrast pixels ("plate-like"). Each camera writes only its own result slot (no sharing); a pool of N threads takes the next unprocessed camera with an atomic counter. The checksum of all results is identical in every configuration (8111236920), so every version computes the same answer [M]. Sequential time per camera = 2625.5 / 64 = 41.0 ms [C].

## 2. Measured speed-up (speedup_results.txt, 5 runs each) [M]
| Configuration | min ms | median ms | max ms | speed-up [C] |
|---|---|---|---|---|
| sequential (1 thread) | 2595.9 | 2625.5 | 2675.4 | 1.00 |
| pool of 2 threads | 1373.9 | 1390.8 | 1414.9 | 1.89 |
| pool of 4 threads | 828.1 | 835.3 | 851.5 | 3.14 |
| pool of 8 threads | 649.1 | 660.8 | 693.4 | 3.97 |
| pool of 16 threads | 607.5 | 665.5 | 688.6 | 3.95 |
| 1 thread per camera (64) | 643.1 | 672.3 | 711.1 | 3.91 |
Speed-up = sequential median / configuration median. Efficiency [C]: 2 threads 94.5 %, 4 threads 78.5 %, 8 threads 49.6 % of the 8 logical CPUs (but 99.3 % of the 4 physical cores).

## 3. Why the speed-up stops increasing
* Hardware limit: the machine has 4 physical cores. The best speed-up (3.97) is about 4, and two hardware threads on one core share its execution units, so the extra 4 logical CPUs added little for this compute-bound loop (3.14 -> 3.97).
* More threads than CPUs only time-share: 16 threads (3.95) and 64 threads (3.91) are no faster than 8 (ranges overlap) [M].
* The serial part is not the cause [C]: creating a thread costs about 79 us (Q1), so 16 threads cost about 1.3 ms = 0.19 % of 665 ms, and 64 threads about 5.0 ms = 0.75 %. Amdahl's law with a 0.2 % serial part would allow about 7.85 on 8 CPUs, well above the measured 3.97.
* Hypothesis, not tested: with 4 threads the OS may place two threads on the same physical core, which could explain 3.14 < 4; background load and clock speeds could also play a part.
* Pool of 8 is the sensible choice here (= number of logical CPUs). One thread per camera gives the same speed with 64 stacks (8 MiB virtual each) [T].

## 4. Shared and private resources (shared_demo_output.txt) [M]
| Resource | Shared or private | Evidence |
|---|---|---|
| Process id | shared | same pid=2156 in main and all threads |
| Global variables (data segment) | shared | same &global in every thread |
| Heap | shared | same heap pointer in every thread |
| Open files (descriptor table) | shared | fd valid in every thread |
| Stack / local variables | private | &local differs in each thread (spaced about 8 MiB apart [C], the default stack size) |
| errno | private | each thread kept its own value (1, 2, 3) |
| Thread id and registers/program counter | private | distinct tid (2158, 2159, 2160); /proc/2156/task lists 4 kernel tasks (main + 3) |
Note: the /proc listing appears before the other lines only because the output was piped to tee (block buffering).

## 5. User-level vs kernel-level threads; thread models
* User-level threads are managed by a library; the kernel sees one process. Kernel-level threads are scheduled by the kernel itself.
* Many-to-one: many user threads on one kernel thread. Cheap switches, but one blocking call blocks all and there is no parallelism on several cores.
* One-to-one: each user thread has its own kernel thread. True parallelism and independent blocking, but each thread costs kernel resources. Linux POSIX threads (NPTL) use this model [T]; consistent with /proc/PID/task showing one kernel task per thread [M] and with the real speed-up measured above.
* Many-to-many: many user threads multiplexed on a smaller number of kernel threads. Flexible but complex; rarely used today [T].

## 6. Shared counter without protection (race_results.txt, 5 runs) [M]
8 threads each add 1 to one shared counter 1,000,000 times (expected 8,000,000).
| Version | result (5 runs) | median time |
|---|---|---|
| unprotected | 1,550,100 to 1,883,464; median lost 6,312,008 (78.9 %) | 71.8 ms |
| mutex | 8,000,000 every run | 290.4 ms |
| atomic | 8,000,000 every run | 94.1 ms |
Why: counter++ is load, add, store; two threads can load the same value and both store value + 1, so one increment is lost. Cost of correctness [C]: mutex 4.0x and atomic 1.3x the time of the (wrong) unprotected version. The camera program avoids the problem by giving each camera its own result slot.

## 7. Alternative concept: processes instead of threads (speedup_results.txt, Task01 results) [M]
| Aspect | Processes | Threads |
|---|---|---|
| Creation per task (Q1) | 137.7 us | 78.8 us |
| Context switch round trip (Q1) | 6.48 us | 5.80 us |
| Memory per idle task (Q1) | 116.0 kB | 30.0 kB |
| 8 workers, wall time | 663.5 ms (shared memory), 667.4 ms (pipe) | 660.8 ms |
| 4 workers, wall time | 917.7 ms (shared memory; range 853 to 1129) | 835.3 ms |
* Wall time: with 8 workers processes are only 0.4 % (shared memory) and 1.0 % (pipe) slower than threads [C]; with 4 workers they were 9.9 % slower, but that run was noisy.
* Memory: separate address spaces cost about 86 kB more per task (Q1), but the frame buffers (2 x 76,800 bytes, about 150 kB per worker [C]) are private in both designs.
* Combining results: threads write into shared result slots, with no copying. Processes need IPC: shared memory (no copy, the method used for "shm") or a pipe (64 records x 16 bytes = 1,024 bytes copied [C], negligible).
* Verdict: for CPU-bound work with small results, both give the same speed here. Threads are simpler; processes give isolation if one camera decoder crashes [T].
Limits: simulated frames, 5 runs per point, WSL2 timing noise.
