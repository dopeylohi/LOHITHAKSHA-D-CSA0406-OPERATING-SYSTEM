# Task04 - Virtual Memory and Demand Paging (8 marks)

Setup: Ubuntu 26.04.1 on WSL2 (8939 MB RAM, 4096 MB swap on /dev/sdc [M]), gcc 15.2.0, `-std=c11 -Wall -Wextra -O2` (build.sh).
Labels: [M] measured, [C] calculated (working shown), [T] textbook fact, [A] assumed.

## 1. Steps of handling a page fault [T]
1. The CPU translates the address; the page-table entry says "invalid / not in memory", so the MMU traps to the OS.
2. The OS checks (usually in the process control block) whether the reference is legal. An illegal one terminates the process (segmentation fault, see Task03); a legal one means the page is just not in memory.
3. The OS finds a free frame (if none, a page-replacement algorithm chooses a victim, and a dirty victim is first written to disk).
4. It schedules a disk read of the page into that frame; the process waits and the CPU runs another process.
5. When the I/O completes, the OS updates the page table (frame number, valid bit set).
6. The faulting instruction is restarted.
7. The process continues as if the page had always been there.
A "minor" fault needs no disk read (new zero-filled page, or the page is already in memory); a "major" fault needs I/O.

## 2. How demand paging lets a 24 GB program run in 16 GB
Pages are loaded only when first touched, and unused or cold pages can be written to swap and their frames reused, so only the working set must fit in RAM, not the whole program [T]. Evidence [M]: touching a fresh 64 MB block (16,384 pages) caused 16,386 minor faults, so each page is created on demand at its first access (the 2 extra faults belong to the program's own start-up), and the second pass over the same pages caused 0 faults (Table 4.1).

## 3. Sequential and random access, page faults counted (touch2.c) - Table 4.1
64 MB = 16,384 pages of 4096 B, RAM plentiful, one run each [M]:
| Order | Pass | Minor faults | Major faults | Time | Per page |
|---|---|---|---|---|---|
| sequential | 1, first touch | 16,386 | 0 | 76.25 ms | 4654.0 ns |
| sequential | 2, second touch | 0 | 0 | 0.36 ms | 21.9 ns |
| random | 1, first touch | 16,386 | 0 | 84.88 ms | 5180.4 ns |
| random | 2, second touch | 0 | 0 | 0.38 ms | 23.3 ns |
- The number of faults is the same for both orders (one per page) because the pages were never in memory before; the order of access does not change how many faults a first touch causes when RAM is large.
- [C] A first-touch (minor) fault costs 4654 / 21.9 = 212 times a plain memory access to a resident page, but it is 8,000,000 / 4654 = 1,719 times cheaper than the 8 ms disk fault assumed in the question, because no disk read is needed.
- Random first touch was 11% slower than sequential ((84.88 - 76.25) / 76.25 = 11.3% [C]); this is one run each, so I do not claim the difference is reliable. The order matters much more when memory is short (Section 9).

## 4. Effective access time (calc.sh) - Table 4.2
EAT = (1 - p) x m + p x S, with m = 100 ns and S = 8 ms = 8,000,000 ns (given) [C]:
| Fault rate p | EAT working | EAT | Slow-down |
|---|---|---|---|
| 1 in 1,000 (0.001) | 0.999 x 100 + 0.001 x 8,000,000 = 99.9 + 8000 | 8099.9 ns | 81.0 times |
| 1 in 100,000 (0.00001) | 0.99999 x 100 + 0.00001 x 8,000,000 = 100.0 + 80 | 179.999 ns | 1.80 times |
| 1 in 1,000,000 (0.000001) | 0.999999 x 100 + 0.000001 x 8,000,000 = 100.0 + 8 | 107.9999 ns | 1.08 times |
Maximum fault rate for a 10% slow-down [C]: (1 - p) x 100 + p x 8,000,000 <= 110, so 100 + p x 7,999,900 <= 110, so p <= 10 / 7,999,900 = 1.25 x 10^-6, about 1 fault in 800,000 accesses. Even one fault per 100,000 accesses is already 80% slower; the disk is so much slower than memory that page faults have to be extremely rare.

## 5. Frames per process: equal and proportional allocation [T, C]
Example [A]: m = 128 free frames, three processes of 10, 40 and 150 pages (S = 200 pages).
- Equal allocation: m / n = 128 / 3 = 42.7 frames each. The 10-page process wastes frames and the 150-page process gets far too few.
- Proportional allocation: a_i = s_i / S x m = 10/200 x 128 = 6.4, 40/200 x 128 = 25.6, 150/200 x 128 = 96.0 frames (sum 128). Better, because larger processes get more, but it ignores priority and actual need.
- Every process needs a minimum number of frames (set by the instruction set architecture, so that one instruction can complete) and should get about its working-set size; with global replacement a process can take frames from others, with local replacement it cannot [T].

## 6. Thrashing and the working-set model [T]
- Thrashing: a process (or the whole system) spends more time paging than executing, because it has fewer frames than its active pages need, so every fault evicts a page that is needed again soon. Detection: CPU utilisation falls while the page-fault rate and disk activity rise; a naive OS then starts more processes (to raise utilisation), which makes it worse.
- Working-set model: working set WS(t, D) = the pages referenced in the last D references; the OS keeps each process's WS in memory. If the sum of all working sets is bigger than RAM the OS suspends (swaps out) a process; if there is spare memory it can start another one. This is based on locality: a program moves through localities (code and data it uses together) and needs them resident.
- Page-fault-frequency control: set an upper and a lower fault-rate bound per process; above the upper bound give it more frames, below the lower bound take frames away, and if no frames are free suspend a process.
- Evidence of thrashing [M]: Section 8.

## 7. Copy-on-write and fork() (cow.c) - Table 4.3
Copy-on-write: fork() does not copy the parent's memory; it copies only the page tables and marks all pages read-only and shared. A page is copied only when one of the processes writes to it (a protection fault, then the kernel copies that one page) [T]. This makes fork() fast, especially when the child calls exec() soon afterwards. 64 MB block written by the parent, values from /proc/self/smaps_rollup [M]:
| Moment | Rss (kB) | Pss (kB) | Private_Dirty (kB) | Shared_Dirty (kB) |
|---|---|---|---|---|
| parent before fork | 67,484 | 65,979 | 65,648 | 0 |
| child after fork, no write | 66,348 | 32,924 | 28 | 65,616 |
| child after writing 1 byte per page | 66,732 | 65,748 | 65,572 | 72 |
| parent, after the child wrote | 67,548 | 65,935 | 65,576 | 72 |
- Right after fork the child's memory is almost entirely shared: Pss is about half of Rss, 32,924 kB against 65,979 / 2 = 32,990 kB [C], so no copy of the 64 MB was made.
- The child's writes caused 16,384 minor faults = 64 MB / 4 KB pages [C], one copy-on-write fault per page.
- After the writes both processes have private copies: 65,572 + 65,576 = 131,148 kB, about 128 MB [C], twice the starting 64 MB; the memory is only used when it is written.
fork() time in the parent, median of 5 runs [M]:
| Block size | 1 MB | 16 MB | 64 MB | 256 MB |
|---|---|---|---|---|
| fork() median | 0.238 ms | 0.338 ms | 1.284 ms | 5.104 ms |
- [C] The block grew 256 times (1 MB to 256 MB) but fork() only 5.104 / 0.238 = 21.4 times, because only the page tables are copied. It is still not free (about 0.020 ms per MB at 256 MB, 5.104 / 256).
- For comparison [A, C]: copying 256 MB at an assumed 10 GB/s would take 25.6 ms, five times more than the measured 5.1 ms. The 256 MB runs varied a lot (3.5 ms to 14.3 ms), so the median is the better figure.

## 8. Run time when memory used grows beyond the limit (memgrow.c, run_limit.sh) - Table 4.4
Method [M]: the program allocates N MB, touches every page once, then makes 3 timed passes over all pages (sequential, or in a fresh random order each pass). It ran inside a cgroup with memory.max = 268,435,456 B (256 MB) and memory.swap.max = 4,294,967,296 B (4 GB), set by `systemd-run --scope -p MemoryMax=256M -p MemorySwapMax=4G` (the program printed these values, so the limit really applied). Free RAM on the machine was about 6.7 GB, so the 256 MB limit stands for "free RAM". Mean of the 3 passes per run; fault counts are for the 3 passes (minor + major):
| Size | Order | Runs finished | Mean pass time per run | Faults (minor / major) |
|---|---|---|---|---|
| 128 MB | seq | 3 of 3 | 0.77, 0.70, 0.80 ms; median 0.77 ms | 0 / 0 |
| 128 MB | rand | 3 of 3 | 0.87, 0.87, 0.93 ms; median 0.87 ms | 0 / 0 |
| 192 MB | seq | 3 of 3 | 1.13, 1.13, 1.13 ms; median 1.13 ms | 0 / 0 |
| 192 MB | rand | 3 of 3 | 1.37, 1.47, 1.27 ms; median 1.37 ms | 0 / 0 |
| 256 MB | seq | 2 of 3 (1 killed) | 6454.2 and 4397.4 ms | 150,433 / 926 and 117,972 / 696 |
| 256 MB | rand | 0 of 3, all killed | - | - |
| 320 MB and 384 MB | seq and rand | 0 of 12, all killed | - | - |
| 512 MB | seq | 1 of 3 (2 killed) | 33,310.1 ms | 336,912 / 56,500 |
| 512 MB | rand | 0 of 3, all killed | - | - |
| 512 MB, no limit | seq | 3 of 3 | 2.97, 2.97, 3.37 ms; median 2.97 ms | 0 / 0 |
| 512 MB, no limit | rand | 3 of 3 | 4.10, 3.80, 3.90 ms; median 3.90 ms | 0 / 0 |
"Killed" = exit code 137, stopped by the kernel's out-of-memory killer although 4 GB of swap was allowed. I did not investigate why (my guess, not verified: the program touched pages faster than the swap device could take them, so the cgroup could not free memory in time). These cases are reported as failures, not as timings.
Working [C]:
- Resident case: 128 MB = 32,768 pages, 0.767 ms / 32,768 = 23.4 ns per page access, the same as the 21.9 ns of Table 4.1; time grows in step with size (192 / 128 = 1.5 times the memory, 1.13 / 0.77 = 1.5 times the time).
- At 256 MB (size equal to the limit, so the program plus its own overhead no longer fits): expected time by scaling 1.13 ms x 256 / 192 = 1.51 ms per pass; measured 4397 to 6454 ms, that is 2,900 to 4,300 times slower. Faults per access: (150,433 + 926) / (3 x 65,536) = 151,359 / 196,608 = 0.770 in run 1 and (117,972 + 696) / 196,608 = 0.604 in run 2. So 60% to 77% of all accesses faulted.
- At 512 MB (twice the limit): faults (336,912 + 56,500) / (3 x 131,072) = 393,412 / 393,216 = 1.0005, so essentially every access faulted. Mean pass 33,310 ms against 2.97 ms without a limit: 11,200 times slower [C]; 33,310 ms / 131,072 accesses = 254 microseconds per access [C]. A sequential cyclic scan over more pages than frames is the worst case for LRU-like replacement: each page is evicted just before it is needed again, which is thrashing.
- Time per fault on this machine [C]: run 1 at 256 MB took 6454.2 ms / 65,536 = 98.5 microseconds per access; (98,500 - 0.23 x 100) / 0.77 = 128 microseconds per fault, about 62 times faster than the 8 ms of the question (8000 / 128), because swap here is a virtual disk on an SSD [A].
- The first pass is the slowest and later passes get faster (11,484.6, 5,061.7, 2,816.3 ms in run 1 at 256 MB); I did not look into why.
- Split of minor and major faults: Linux counts a fault as major when it reads the swap device and as minor when the page is still in the swap cache [T], so the minor count is large here, but the total number of faults is what matters.

## 9. Page faults against the number of frames (fault_curve.c) - Figure 4.1, Table 4.5
Reference string [A, M]: the pages touched by one pass over a 256 x 256 int matrix (256 KB = 64 pages of 4096 B; 65,536 references), row-major (a[i][j] with j in the inner loop) and column-major (i inner). LRU and FIFO were simulated for 1 to 72 frames. Results from fault_curve.csv [M]:
| Traversal | Frames | LRU faults | FIFO faults |
|---|---|---|---|
| row-major | 1 to 72 (all values) | 64 | 64 |
| column-major | 1 to 63 | 16,384 | 16,384 |
| column-major | 64 to 72 | 64 | 64 |
- Row-major: one page holds 4 rows (1024 ints), so each page is used 1024 times in a row; one frame is enough and only the 64 first-touch faults happen.
- Column-major [C]: each column visit sweeps all 64 pages, using each for 4 consecutive references, so 64 faults per column x 256 columns = 16,384 faults when the pages do not all fit. With 63 frames or fewer a cyclic sweep over 64 pages makes both LRU and FIFO evict the page needed next; with 64 frames everything stays resident, and the faults fall from 16,384 to 64, a factor 256 at one extra frame. The curve has a sharp "knee" at the working-set size (64 pages).
- The same total memory traffic produces 256 times more faults only because of the order of access: this is what locality means.

## 10. Conclusion: more RAM or better locality for the radiologist?
Fix the locality first, buy RAM only if the working set still does not fit.
- Evidence [M, C]: the faults depend on the access pattern and on whether the working set fits: with a good pattern (row-major) 1 frame was enough, with a bad one (column-major) 63 frames gave 256 times more faults, and a cyclic scan twice as large as the limit made every access fault (254 microseconds per access against 0.023 microseconds resident, Section 8).
- The acceptable fault rate is extremely low: 1 in 800,000 for a 10% slow-down (Section 4); an image program that sweeps the whole 24 GB over and over will be far above that, as my 512 MB test (rate about 1.0) shows. Reordering the loops, or processing the image in tiles or slabs that fit in RAM (blocking), is free and can bring the working set under 16 GB.
- If the algorithm really needs random access to all 24 GB, locality cannot be fixed, and then RAM is the answer: 32 GB (Section 11).
So: first restructure (cost: programmer time, no hardware), measure the fault rate with /usr/bin/time -v or getrusage, and buy RAM if the rate stays above about 1 in 800,000.

## 11. Alternative: 32 GB of RAM instead of demand paging
- Solution: all 24 GB stay resident, so after the first touch there are no major faults and the access time is the plain memory time.
- Effective access time for the measured fault rate (256 MB run 1, p = 0.770 [C]): with demand paging and the question's S = 8 ms, EAT = 0.23 x 100 + 0.77 x 8,000,000 = 6,160,023 ns = 6.16 ms; with enough RAM p is about 0 and EAT is 100 ns. The ratio is 6,160,023 / 100 = 61,600 times [C]. Using the machine's own measured fault cost instead (128 microseconds) the demand-paged EAT would be 0.23 x 100 + 0.77 x 128,000 = 98.6 microseconds, 986 times slower than 100 ns [C]. That p is for my worst-case cyclic test, a well-organised program would have a much smaller p.
- Cost against slowness [A]: assume a job that takes 10 minutes with enough RAM; at the 81 times slow-down of p = 1 in 1,000 (Table 4.2) it takes 810 minutes = 13.5 hours [C]. If extra RAM costs a few thousand rupees (price to be checked, assumed), it is cheaper than a radiologist waiting for hours on every study.
- Space: demand paging needs 16 GB RAM + 8 GB swap (24 GB of memory and disk traffic); 32 GB needs 24 GB RAM and no swap traffic. Time: demand paging is O(1) per hit and a disk access (milliseconds) per fault; resident memory is O(1) per access.

## 12. Limits
- The memory-limit test: 22 of 30 limited runs were killed, only 3 runs finished with a real slow-down and each of those was a single run per configuration, below the 5 repeats that I wanted. The numbers show the collapse clearly, but exact values are not repeatable.
- The "free RAM" is a cgroup limit of 256 MB, not a real shortage of memory, and the swap is a virtual disk on an SSD, so fault costs are much lower than a hard disk's 8 ms.
- The fault-vs-frames curve is a simulation of a 256 KB array, scaled down from the radiologist's 24 GB.
