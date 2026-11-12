# Q5 – Synchronisation: railway ticket booking (notes)
Labels: [M] measured on my machine, [C] calculated from [M], [T] typical published/general knowledge.
Machine: Ryzen 5 7235HS (4 cores, 8 logical CPUs), WSL2, gcc -O2 -pthread. Each timing = 5 runs (median shown).

## 1. From the three requirements to a design
A critical-section solution must give: mutual exclusion (never two agents in the seat-update code), progress (if nobody is inside, a waiting agent is not blocked by agents that are not interested), bounded waiting (an agent waits only a limited number of turns).
Design: protect only the shared state that must change together (the seat table of one coach) with a lock; keep slow work outside it; use semaphores to count resources (free seats, free buffer slots) and a binary lock (mutex) for exclusive updates; use a monitor (mutex + condition variables) when a thread must also wait for a condition. POSIX does not guarantee first-in-first-out wake-up for semaphores [T], so bounded waiting depends on the system's fair queuing.

## 2. Peterson's solution (two threads) [M]
~~~
lock(i):  j = 1-i;  flag[i] = 1;  turn = j;  while (flag[j] && turn == j) ;   unlock(i): flag[i] = 0
~~~
* Mutual exclusion: both threads can be inside only if flag[0] = flag[1] = 1 and neither is blocked; but turn holds one value, so one of the loop conditions is false for exactly one thread and the other waits.
* Progress: if the other thread is not interested (flag = 0) the loop ends at once; if both are interested, turn decides, so one of them proceeds.
* Bounded waiting: when a thread leaves it sets flag = 0; if it comes back it sets turn to the other thread first, so a waiting thread enters after at most one turn of the other.
* Why it can fail on modern processors: the proof assumes memory operations happen in program order. CPUs keep a store buffer and may let the load of flag[j] pass the earlier store to flag[i] (store-to-load reordering, allowed on x86); compilers also reorder. Both threads can then read 0 and enter together.
* Measured (peterson_results.txt, 2 threads x 1,000,000 increments, 5 runs): plain volatile version lost 1,697 to 3,653 updates per run (median 2,466; 0.12 %), median time 66.1 ms; the sequentially consistent atomic version lost 0 in every run, median time 158.4 ms (2.4x slower) [M][C].

## 3. Semaphores protecting the seat counter
~~~
sem_t seats_left;   /* counting semaphore, starts at the number of free seats */
sem_t m;            /* binary semaphore (mutex), starts at 1                   */
book():   if (sem_trywait(&seats_left) != 0) return SOLD_OUT;   /* takes one seat from the counter */
          sem_wait(&m);  choose a seat; mark it sold;  sem_post(&m);
cancel(): sem_wait(&m);  mark the seat free;  sem_post(&m);  sem_post(&seats_left);
~~~
The counting semaphore makes sure no more tickets are sold than seats exist (the count can never go below 0); the binary semaphore makes the choose-and-mark step exclusive, so two agents cannot take the same seat. The measured need: without any lock the benchmark oversold up to 993 seats (section 7).

## 4. Classic problems with semaphores (classic_results.txt, multi_100runs.txt) [M]
* Bounded buffer: semaphore empty (starts at SIZE), semaphore full (starts at 0), mutex around the index update. Order matters: wait on empty/full BEFORE taking the mutex, otherwise a thread sleeps holding the mutex and everything deadlocks.
* Readers-writers (readers' priority): mutex for read_count and semaphore wrt. The first reader locks wrt, the last reader releases it. Result: with semaphores 31,357 reads, 2,502 writes and 0 inconsistent reads; with no synchronisation 39,340 of 40,731 reads (96.6 %) saw a half-finished update (A not equal to B). Writers got fewer turns with the semaphores (2,502 against 9,486), which shows the readers' priority; writer starvation was not tested.
* Dining philosophers (5 forks, one semaphore per fork): the naive left-then-right order deadlocked in all 3 runs (nobody ate, 0 meals). Two fixes worked: let the last philosopher take the right fork first (breaks the circular wait): 6,229 meals in 3 s, but philosophers 0 and 4 ate about half as often (786 against up to 1,673); or let at most 4 sit down (room semaphore): 7,678 meals, evenly shared (1,524 to 1,540 each).

## 5. Monitor with condition variables
A monitor keeps the data (buffer, count) and the lock in one place; threads wait on a condition inside a while loop (while count == SIZE wait(not_full)) and signal after changing the state. It is easier to get right because there are no counters to initialise, the same lock protects the data and the condition, and the lock is released automatically while waiting, so the "wait before lock" ordering mistake cannot happen. Evidence: the monitor bounded buffer passed 100 of 100 runs with 1 and with 4 producers/consumers (section 6).

## 6. Bounded-buffer test: given program and many threads
* Given program (TODO 1 and 2 completed): "items consumed: 20, sum = 210 (expected 210)"; 100 runs: 100 times the same line [M].
* 1 and 4 producers/consumers (each item has a unique id; the checker counts how often each id was consumed; 20,000 items per producer; 100 runs each) [M]:
| Implementation | 1 producer, 1 consumer | 4 producers, 4 consumers |
|---|---|---|
| semaphores + mutex | 100/100 pass | 100/100 pass |
| monitor | 100/100 pass | 100/100 pass |
| semaphores, mutex removed | 100/100 pass | 0/100 pass (last failure: 10,437 of 80,000 ids missing, 10,373 consumed twice; sum 3,194,881,887 instead of 3,200,040,000) |
"Pass" means every id was consumed exactly once and the sum was correct.
* Why removing the mutex is safe only for one producer and one consumer: then only the producer writes "in" and only the consumer writes "out", and the semaphores already guarantee that a slot is written only when empty and read only when full. With several producers two of them can read the same "in", write the same slot (an item is overwritten: missing) and several consumers can read the same "out" (an item taken twice: duplicated).

## 7. Deployment decision: one lock, per train or per coach (booking_results.txt) [M]
Benchmark: 64 trains x 16 coaches x 256 seats; 200,000 booking attempts spread over the threads; 80 % of the work sits inside the critical section (WORK_IN 800 against WORK_OUT 200 loop steps; assumed). Median bookings per second (min to max in the results file):
| Lock | 1 thread | 4 threads | 8 threads | 16 threads | oversold seats (max over 5 runs, 4/8/16 threads) |
|---|---|---|---|---|---|
| none (unsafe) | 1,003,517 | 3,867,257 | 4,902,209 | 5,630,771 | 467 / 877 / 993 |
| one global lock | 996,730 | 493,822 | 475,193 | 533,674 | 0 |
| one lock per train (64) | 988,823 | 2,447,340 | 3,414,098 | 3,812,834 | 0 |
| one lock per coach (1,024) | 990,810 | 3,628,663 | 4,173,759 | 4,451,656 | 0 |
Calculated from these [C]:
* The global lock is slower than a single thread (0.48x at 8 threads, 0.54x at 16): 80 % of the work is serialised, so by Amdahl's law even an ideal result would be only 1.23x at 16 threads; the rest is lock contention (a hypothesis: sleeping and waking threads), not measured separately.
* Speed-up over one thread at 16 threads: per train 3.86x, per coach 4.49x. Per coach is 8.3x the global lock and 1.17x the per-train lock.
* The unsafe version is only 26 % faster than per-coach locking at 16 threads (5.63 M against 4.45 M per second; correctness costs 21 % of the unsafe speed) and oversold up to 993 seats (about 0.5 % of 200,000 bookings at 16 threads).
* Lock memory: 1,024 padded locks of 104 bytes = 106,496 bytes (about 104 KB); 64 train locks = 6,656 bytes.
Decision: one lock per coach. It keeps the seat check-and-mark exclusive (0 oversold in 60 runs) and gives the highest throughput. Conditions: an operation touching several coaches (group booking, whole-train changes) must take the locks in a fixed order (lower coach number first) to avoid deadlock [T]; if that complexity is unwanted, the per-train lock gives 86 % of the per-coach throughput (3.81 M against 4.45 M). Limits: simulated work, 5 runs, WSL2 timing noise, random coach choice (no hot-spot trains).

## 8. Alternative concept: atomic operations or a spin lock (counter_results.txt) [M]
* Solution: C11 atomic_fetch_add updates one counter word without any lock; a spin lock (atomic_flag test-and-set) busy-waits in a loop instead of sleeping. Atomics only suit single-word updates; the multi-step seat choice needs a lock.
* Median time per update (ns) and CPU time / wall time (cpu/wall):
| Critical section | Threads | mutex | spin lock | atomic add |
|---|---|---|---|---|
| short (counter only) | 1 | 8.1 (0.82) | 3.8 (0.67) | 2.3 (0.45) |
| short | 4 | 19.5 (2.95) | 51.8 (3.75) | 12.0 (3.56) |
| short | 16 | 38.7 (7.25) | 204.2 (7.69) | 14.7 (5.46) |
| long (about 20 us of work inside) | 1 | 19,914 (0.97) | 19,506 (0.97) | not applicable |
| long | 4 | 34,163 (1.12) | 19,571 (2.88) | not applicable |
| long | 16 | 34,524 (1.10) | 28,449 (6.92) | not applicable |
* Short sections: atomics are best; at 16 threads atomic is 2.6x faster than the mutex and the spin lock is 5.3x slower than the mutex [C] (the spinning threads outnumber the 8 CPUs, and a thread that holds the lock may be descheduled while the others burn CPU).
* Wasted CPU: a long critical section with 4 threads keeps 2.88 CPUs busy with the spin lock but only 1.12 with the mutex; at 16 threads 6.92 against 1.10. The spin lock is only about 18 % faster there (28,449 against 34,524 ns) while using about 6x the CPU [C]; the mutex sleeps and leaves the CPUs free.
* Which suits a long critical section: the mutex (or semaphore), because sleeping costs about 14 us of hand-over time per update here [C] but wastes no CPU. A spin lock suits only a very short section with few threads (1 thread: 3.8 against 8.1 ns).
* The cpu/wall values below 1 for one thread are due to the main thread waiting in join.
