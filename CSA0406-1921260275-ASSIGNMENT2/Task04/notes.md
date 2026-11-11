# Q4 – Inter-Process Communication: weather station pipeline (notes)
Labels: [M] measured on my machine, [C] calculated from [M], [T] typical published/general knowledge.
Machine: Ryzen 5 7235HS, WSL2, gcc -O2 for the transfer benchmark.

## 1. How a pipe signals end of data [M]
read() on a pipe returns 0 (end of file) only when every copy of the write end is closed. In pipe_ok.c the child closes its own copy of fd[1] at the start, and the parent closes its copy after the last write; then the child's read returns 0 and its loop ends. Output (TODO 1 completed): "analyser: 6 readings, average 35.5, max 41" and "reader: done" (matches the expected output).

## 2. Removing close(fd[1]) in the parent (pipe_noclose) [M]
The parent still holds the write end open, so the kernel never reports end of file: the child's read() blocks, waiting for data that can never come, while the parent waits in waitpid() for the child. Both wait for each other (deadlock). ps showed the parent in state S, WCHAN do_wait, and the child in state S, WCHAN anon_pipe_read. When I killed the parent, its write end was closed, the child got end of file and printed "analyser: 6 readings, average 35.5, max 41" - which confirms the explanation.

## 3. Shared memory vs message passing
* Message passing (pipe, message queue): the kernel carries every message; the sender calls write/send and the receiver read/receive. Simple and safe, the kernel also does the blocking.
* Shared memory: both processes map the same memory and read/write it directly; the kernel is involved only in setting it up. Fast, but the programmer must provide synchronisation.

## 3b. Which method copies the data, and which needs extra synchronisation
* Pipe: two copies through the kernel [T]: user buffer -> kernel pipe buffer on write, kernel -> reader's buffer on read (200 MiB copied for a 100 MiB transfer [C]). Synchronisation is built in: the kernel blocks a writer when the pipe is full and a reader when it is empty.
* Shared memory: no copy; the reader uses the data in place. No built-in synchronisation, so semaphores (or similar) are required.
* Evidence (nosync_results.txt, 5 runs) [M]: with shared memory and no semaphore the writer published 1,000,000 readings but the reader saw only 4,044 to 5,874 different ones; median lost 994,980 (99.5 %). With two counting semaphores (shm_sem) all 6 of 6 readings arrived: "analyser: 6 readings, average 35.5, max 41".

## 4. Growth investigation: 1, 10 and 100 MiB (transfer_results.txt, 5 runs each) [M]
| Size | Pipe min / median / max (ms) | Shared memory min / median / max (ms) | Pipe / shm [C] | Pipe rate [C] | Shm rate [C] |
|---|---|---|---|---|---|
| 1 MiB | 0.976 / 1.197 / 1.455 | 1.040 / 1.099 / 1.222 | 1.09x | 835 MiB/s | 910 MiB/s |
| 10 MiB | 7.875 / 8.055 / 13.113 | 1.257 / 1.378 / 1.976 | 5.85x | 1,241 MiB/s | 7,257 MiB/s |
| 100 MiB | 84.145 / 88.612 / 98.496 | 5.786 / 6.364 / 7.194 | 13.92x | 1,128 MiB/s | 15,713 MiB/s |
The checksum was verified in the benchmark program, and a separate 100 MiB run printed "checksum OK" for both methods (that run took 108.7 ms for the pipe and 8.0 ms for shared memory; it is not part of the table).
* Both grow linearly with size, i.e. O(n) [C]; between 10 and 100 MiB the marginal cost is 0.895 ms per MiB for the pipe and 0.055 ms per MiB for shared memory (16x lower) [C].
* At 1 MiB the two methods are within noise (ranges overlap, 1.09x); the advantage appears as the size grows (5.85x at 10 MiB, 13.92x at 100 MiB).
* The pipe moves data in 64 KiB chunks [T]: 100 MiB is 1,600 chunks, 55 us per chunk for the pipe against 4.0 us for shared memory [C]. A memory copy of 64 KiB should be much faster than 55 us [T], so wake-ups and context switches between the two processes probably dominate on WSL2 - this is a hypothesis, not measured.
Assumptions: chunk size 64 KiB; shared ring of 16 chunks; sender and receiver generate and sum the data in both methods, so only the transfer differs; timing taken from the first write to the last byte received.

## 5. Alternative concept and complexity: shared memory + semaphore
Solution (shm_sem.c): a ring of 4 slots in shared memory, a semaphore "empty" (starts at 4) and "full" (starts at 0). Producer: wait(empty), store reading, post(full). Consumer: wait(full), load reading, post(empty). A sentinel value (-1) marks the end of data. Output equals the pipe version [M].
* Time for n readings: both O(n). The pipe copies every byte twice (2 x 4n bytes) plus a system call per write/read; shared memory copies nothing, and semaphore operations are cheap when they do not block [T]. Measured marginal cost: 0.895 ms/MiB (pipe) against 0.055 ms/MiB (shared memory) [C].
* Space: O(1) extra for a fixed ring (4 slots here, 1 MiB ring in the benchmark); the pipe uses a kernel buffer of 64 KiB [T].
* Synchronisation: one producer and one consumer need only the two counting semaphores (no mutex); several producers or consumers would also need a mutex around the in/out indexes [T].

## 6. Choice of method
For the weather station (a few small readings per second) I choose the pipe: shorter code, kernel-managed blocking and end-of-data (EOF), no way to lose readings (3 above), and the speed difference is irrelevant at small sizes (1.09x at 1 MiB). If the station later sends tens of MiB per transfer, shared memory with semaphores is 5.9x faster at 10 MiB and 13.9x at 100 MiB [M], and the extra synchronisation code is then worth it.
