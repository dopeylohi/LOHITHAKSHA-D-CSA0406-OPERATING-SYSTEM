# Task02 - Paging (8 marks)

Setup: Ubuntu 26.04.1 on WSL2, gcc 15.2.0, flags `-std=c11 -Wall -Wextra -O2` (build.sh).
Labels: [M] measured/run, [C] calculated (working shown), [T] textbook fact, [A] assumed.

## 1. External fragmentation (contiguous allocation) - Figure 2.1
Illustration [A], memory 120 KB, sizes in KB:

    | OS 8 | P1 20 | hole 12 | P2 16 | hole 16 | P3 24 | hole 20 | P4 4 |

- [C] Total free = 12 + 16 + 20 = 48 KB, largest hole = 20 KB.
- A new 32 KB program cannot load, because no single hole is 32 KB, although 48 KB is free. This is external fragmentation: free memory exists but is split into unusable pieces. This is what happened on the college server.
- With paging [C]: 32 KB / 4 KB = 8 pages; the 12 + 16 + 20 KB holes contain 3 + 4 + 5 = 12 free 4 KB frames; 8 <= 12, so the program loads, its pages scattered over the frames.

## 2. Paging removes external fragmentation, internal remains
- Memory is cut into equal frames and the program into equal pages. Any page fits any free frame, so there are no unusable holes [T].
- The last page of a process is usually only partly used, and the unused part of that frame is wasted: internal fragmentation, on average half a page per process [T]. Measured in Section 6.

## 3. Address translation with a page table (completed TODO 1)
Logical address a: p = a / PAGE_SIZE, d = a mod PAGE_SIZE. If p >= 8, invalid address; if page_table[p] = -1, page fault; else physical = frame * PAGE_SIZE + d. Code: paging.c (only the TODO was replaced).

PAGE_SIZE 1024 [M], all six lines equal the expected output in the question paper:
| Logical | Page | Offset | Result |
|---|---|---|---|
| 0 | 0 | 0 | physical 5120 |
| 1500 | 1 | 476 | physical 2524 |
| 3100 | 3 | 28 | physical 7196 |
| 6200 | 6 | 56 | physical 3128 |
| 9000 | 8 | 808 | invalid address |
| 2049 | 2 | 1 | page fault |

PAGE_SIZE 2048: hand working first [C], then program output [M] (same):
| Logical | Working | Result |
|---|---|---|
| 0 | p = 0, d = 0, frame 5: 5*2048 + 0 | 10240 |
| 1500 | p = 0, d = 1500, frame 5: 10240 + 1500 | 11740 |
| 3100 | p = 1, d = 3100 - 2048 = 1052, frame 2: 4096 + 1052 | 5148 |
| 6200 | p = 3, d = 6200 - 6144 = 56, frame 7: 14336 + 56 | 14392 |
| 9000 | p = 4, d = 9000 - 8192 = 808, frame 0: 0 + 808 | 808 |
| 2049 | p = 1, d = 1, frame 2: 4096 + 1 | 4097 |
- Observation: with larger pages the same 8-entry table covers 16384 bytes, so 9000 is now valid (before: invalid address) and 2049 maps to page 1 (frame 2), so no page fault happens now.

## 4. Boundary tests (edge_test.c, PAGE_SIZE 1024) [M]
| Address | Meaning | Result |
|---|---|---|
| 1024 | first byte of page 1 | page 1 offset 0 : physical 2048 |
| 2047 | last byte of page 1 | page 1 offset 1023 : physical 3071 |
| 6144 | first byte of page 6 | page 6 offset 0 : physical 3072 |
| 7167 | last
| 8191 | last byte of last page (page 7, not in memory) | page fault |
| 8192 | first byte beyond the last page | page 8 : invalid address |
- Frame 2 starts at 2048 and frame 3 at 3072: the last byte of page 1 (3071) is just before frame 3, so the offset never crosses into another frame. Address 8192 fails the p >= PAGES check; this is the protection check by the page-table length (Section 9).

## 5. Page-table size and effective access time (calc_out.txt)
Page-table size, 4 KB pages, single-level [C]:
- 32-bit: 2^(32-12) = 2^20 = 1,048,576 entries x 4 B [A] = 4,194,304 B = 4 MiB per process.
- 48-bit: 2^(48-12) = 2^36 = 68,719,476,736 entries x 8 B [A] = 512 GiB per process. Impossible to keep for every process, which is why 48-bit systems use multi-level tables (4 levels of 9 bits). A process touching one page needs only 4 x 4 KB = 16 KB of tables [C].

Effective access time, memory m = 100 ns, TLB lookup t = 10 ns [A], hit ratio h [C]:
- Without TLB: one memory access for the page table + one for the data: 2m = 200 ns (4-level table: 5m = 500 ns).
- With TLB: EAT = h(t + m) + (1 - h)(t + 2m):
| Hit ratio | One-level EAT | 4-level EAT |
|---|---|---|
| 80% | 0.80 x 110 + 0.20 x 210 = 130 ns | 0.80 x 110 + 0.20 x 510 = 190 ns |
| 95% | 0.95 x 110 + 0.05 x 210 = 115 ns | 0.95 x 110 + 0.05 x 510 = 130 ns |
| 99% | 0.99 x 110 + 0.01 x 210 = 111 ns | 0.99 x 110 + 0.01 x 510 = 114 ns |
- Conclusion: at 99% the TLB brings the one-level EAT from 200 to 111 ns (1.8 times faster); with 4-level tables the TLB matters even more (500 to 114 ns, 4.4 times).

## 6. Internal fragmentation, 100 random processes (frag.c, 5 seeds) [M]
Assumption [A]: sizes uniform random between 1 byte and 10 MB, deterministic generator (xorshift32), seeds 1 to 5. Mean over 5 seeds:
| Page size | Avg waste per process | Page/2 [C] | Waste as % of allocated memory | Page-table entries for 100 processes |
|---|---|---|---|---|
| 1 KB | 514.9 B | 512 B | 0.0098% | 515,503 |
| 4 KB | 2085.7 B | 2048 B | 0.0396% | 128,914 |
| 2 MB | 1,063,547.7 B (about 1.01 MiB) | 1,048,576 B | 16.79% | 302 |
- The measured waste is close to the theory of half a page per process.

## 7. Evaluation of page sizes 1 KB, 4 KB, 2 MB
Table size for a whole address space (calc_out.txt, B) [C]: 32-bit at 4 B per entry: 16384 KiB (1 KB), 4096 KiB (4 KB), 8 KiB (2 MB); 48-bit at 8 B per entry: 2048 GiB, 512 GiB, 1 GiB.
- 1 KB: least internal fragmentation (0.0098%) but 4 times more table entries than 4 KB (515,503 vs 128,914 for the 100 processes [M]) and each TLB entry covers little memory.
- 4 KB: waste 0.0396% [M]; the usual compromise and the Linux page size on this machine (getconf PAGESIZE = 4096 [M]).
- 2 MB: only 302 entries for 100 processes [M] and one TLB entry covers 512 times more memory than a 4 KB entry [C]; with 64 TLB entries [A] the reach is 64 x 4 KB = 256 KiB against 64 x 2 MiB = 128 MiB [C]. But 16.79% of memory is wasted for these process sizes [M].

## 8. Page size and page faults on Linux [M]
- getconf PAGESIZE = 4096.
- /usr/bin/time -v ./paging (tiny program): 77 minor faults, 0 major, max resident 1784 kB.
- /usr/bin/time -v ./touch 64 (writes one byte in each page of a 64 MB block): the program counted 16384 minor faults and 0 major in the touching loop; /usr/bin/time reported 16464 minor, 0 major, max resident 67448 kB.
- [C] 64 MB / 4096 B = 16384 pages = exactly the counted faults: every page's first touch causes one fault (the page is created on demand). 16464 - 16384 = 80 other faults at start-up, similar to the 77 of the tiny program. 67448 kB is about 65536 kB + 1912 kB of start-up memory.
- A minor fault needs no disk read; a major fault does. All are minor here because nothing was swapped.

## 9. Hierarchical, hashed and inverted page tables [T]
- Hierarchical (multi-level): the page number is split into several indices (x86-64: 4 levels of 9 bits). Unused regions have no lower tables, so a small process uses little table memory (16 KB minimum [C]). Cost: 4 memory accesses per TLB miss.
- Hashed: the page number is hashed to a bucket with a chain of (page, frame) entries; suited to sparse address spaces larger than 32 bits; one hash lookup on average.
- Inverted: one entry per physical frame holding (process id, page number); table size depends on RAM only; lookup by hashing (pid, page); sharing a frame between processes is harder because a frame has one entry.

## 10. Protection and shared pages with paging [T]
- Each page-table entry has a valid/invalid bit and read/write/execute bits. A write to a read-only page or an access to an invalid page causes a trap to the OS.
- A page-table length register stops addresses beyond the table: my test of address 8192 gave "invalid address" [M].
- Shared pages: two processes' page tables point to the same frame (for example shared library or compiler code, read-only). Evidence from Task01: two copies of the dynamic program shared libc code pages (Shared_Clean 1248 kB each, Pss 52 kB) [M].

## 11. Conclusion - page size for the college server
Choose 4 KB pages. Hundreds of student programs of very different sizes, mostly small, are the case where 2 MB pages waste memory (16.79% for my random sizes [M, A]) and 1 KB pages make tables 4 times larger [M]. 4 KB wastes only 0.0396% [M] and is the hardware's default, with the TLB giving 111 ns at 99% hits [C, A]. Optionally, 2 MB huge pages could be used for the few very large jobs, not for everyone [A].

## 12. Alternative: inverted page table instead of per-process multi-level tables
- Solution: one global table, one entry per frame (pid, page, chain pointer). Translation: hash(pid, page), follow the chain until the pid and page match; the index of the entry is the frame number.
- Space [C][A]: 16 GB RAM, 4 KB pages gives 4,194,304 frames x 16 B entry = 64 MiB for the whole machine, whatever the number of processes (O(F), F = frames). Per-process multi-level tables are O(pages in use) for each process, from 16 KB minimum, and grow with every new process; a single-level 48-bit table would be 512 GiB each.
- Time [T]: multi-level walk = L memory accesses on a TLB miss (L = 4, constant); inverted = about 1 hash and a short chain on average (O(1) expected), O(chain length) worst case. I did not measure translation time, so no measured numbers are claimed.
- Weakness: sharing a frame between processes is awkward (one entry per frame), and a hash table is needed to avoid searching all frames.

## 13. Limits
- EAT uses assumed times (m = 100 ns, t = 10 ns) and an assumed 4/8 byte entry size.
- Fragmentation depends on the assumed size distribution (uniform 1 B to 10 MB), not on real data.
- Page-fault numbers are from one run each of /usr/bin/time.
