# Task05 - Page-Replacement Techniques (8 marks)

Setup: Ubuntu 26.04.1 on WSL2, gcc 15.2.0, `-std=c11 -Wall -Wextra -O2` (build.sh).
Labels: [M] measured, [C] calculated, [T] textbook fact, [A] assumed.

## 1. Complete the code - LRU (lru_complete.c)
TODO 1 replaced only; the rest of the program is unchanged. Output [M], identical to the expected output of the paper:
    3 frames: FIFO 15, LRU 12, Optimal 9
    4 frames: FIFO 10, LRU 8, Optimal 8
    Belady string, FIFO: 3 frames 9 faults, 4 frames 10 faults
LRU rule: if a frame is free use it, otherwise replace the frame with the smallest last[] (oldest last use) and set last[] = i.

## 2. Hand verification of one string (the 20-reference string, 3 frames) - Table 5.1
String: 7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1 (index 0 to 19).
FIFO, queue with the oldest page first (F = fault) [C]:
7 F (7); 0 F (7 0); 1 F (7 0 1); 2 F evict 7 (0 1 2); 0 hit; 3 F evict 0 (1 2 3); 0 F evict 1 (2 3 0); 4 F evict 2 (3 0 4); 2 F evict 3 (0 4 2); 3 F evict 0 (4 2 3); 0 F evict 4 (2 3 0); 3 hit; 2 hit; 1 F evict 2 (3 0 1); 2 F evict 3 (0 1 2); 0 hit; 1 hit; 7 F evict 0 (1 2 7); 0 F evict 1 (2 7 0); 1 F evict 2 (7 0 1). Faults = 15.
LRU, pages ordered from least to most recently used [C]:
7 F; 0 F; 1 F; 2 F evict 7 (0 1 2); 0 hit (1 2 0); 3 F evict 1 (2 0 3); 0 hit (2 3 0); 4 F evict 2 (3 0 4); 2 F evict 3 (0 4 2); 3 F evict 0 (4 2 3); 0 F evict 4 (2 3 0); 3 hit (2 0 3); 2 hit (0 3 2); 1 F evict 0 (3 2 1); 2 hit (3 1 2); 0 F evict 3 (1 2 0); 1 hit (2 0 1); 7 F evict 2 (0 1 7); 0 hit (1 7 0); 1 hit (7 0 1). The faults are on 7, 0, 1, 2, 3, 4, 2, 3, 0, 1, 0, 7 = 12.
Optimal, victim = the frame whose next use is farthest [C]:
7 0 1 F (3 faults); 2 F: next uses 7 at 17, 0 at 4, 1 at 13 so evict 7; 0 hit; 3 F: 2 at 8, 0 at 6, 1 at 13 so evict 1; 0 hit; 4 F: 2 at 8, 0 at 10, 3 at 9 so evict 0; 2 hit; 3 hit; 0 F: 2 at 12, 3 at 11, 4 never so evict 4; 3 hit; 2 hit; 1 F: 2 at 14, 0 at 15, 3 never so evict 3; 2, 0, 1 hit; 7 F: 2 never, 0 at 18, 1 at 19 so evict 2; 0, 1 hit. Faults = 9.
Program trace (./experiment trace, Figure 5.1) [M]: FIFO 15, LRU 12, Optimal 9, Clock 14. Hand and program agree, and the final frame slots agree too (FIFO and Optimal end with 7 0 1, LRU with 1 0 7).

## 3. Why FIFO has more faults with 4 frames than 3 (Belady's anomaly)
String 1 2 3 4 1 2 5 1 2 3 4 5 [M]: FIFO 3 frames 9 faults, 4 frames 10 faults.
- 3 frames [C]: faults on 1 2 3 4 1 2 5 3 4 = 9.
- 4 frames [C]: after 1 2 3 4 are loaded, 1 and 2 hit; 5 evicts 1 (the oldest), then 1 evicts 2, 2 evicts 3, 3 evicts 4, 4 evicts 5, 5 evicts 1: faults on 1 2 3 4 5 1 2 3 4 5 = 10.
- Reason: FIFO evicts by arrival time, not by use. In this cyclic string the page that arrived first is exactly the page needed next, so each fault removes the page about to be used. The set of pages in 4 frames is not a superset of the set in 3 frames, so more memory can give more faults.
Why LRU cannot do this (stack property) [T]: with n frames LRU holds the n most recently used distinct pages. That set is always contained in the set of the n + 1 most recently used pages, which is what LRU holds with n + 1 frames. So every hit with n frames is also a hit with n + 1 frames, and faults can never increase. Evidence [M]: on the same string LRU gives 10 faults with 3 frames and 8 with 4 frames; and on both generated strings the check printed "LRU faults never increase with more frames: YES" for frames 3 to 10. FIFO did not show an increase on my generated strings (0 of 7 steps), so Belady's anomaly is rare: only the special string shows it.

## 4. Optimal: best possible but impossible
Optimal evicts the page whose next use is farthest in the future; it gives the minimum number of faults [T]. It needs the future reference string, which the OS cannot know, so it cannot be built; it is used as the lower bound to judge other algorithms. In my results it is the lowest in every row (Tables 5.2 and 5.3).

## 5. Clock (second chance) and hardware support for LRU
- LRU needs hardware to record the order of use on every memory reference: either a counter value copied into the page-table entry at each access (the OS then searches for the smallest), or a stack/list updated at each access (about 6 pointer changes). Doing it in software would need a trap on each reference, which is far too slow [T].
- Clock needs only one reference bit per frame, which the MMU sets for free when the page is used. Frames are in a circle with a hand: on a fault the hand skips frames with bit 1 (clearing them) and replaces the first frame with bit 0. A page used since the last sweep gets a "second chance"; if all bits are 1 it degenerates to FIFO [T].

## 6. Fault counts for locality and random strings - Tables 5.2 and 5.3, Figures 5.2 and 5.3
Strings (10,000 references each) [A]: universe 50 pages; random string = uniform over the 50 pages; locality string = phases of 1000 references with a working set of 6 pages, 90% of references inside the working set and 10% random; deterministic generator (xorshift32). Faults [M] (results.csv):
| Frames | Loc FIFO | Loc LRU | Loc Clock | Loc Optimal | Rand FIFO | Rand LRU | Rand Clock | Rand Optimal |
|---|---|---|---|---|---|---|---|---|
| 3 | 5898 | 5812 | 5888 | 3876 | 9421 | 9408 | 9419 | 7914 |
| 4 | 4729 | 4535 | 4653 | 2693 | 9215 | 9205 | 9208 | 7332 |
| 5 | 3758 | 3353 | 3579 | 1847 | 8997 | 9002 | 8999 | 6837 |
| 6 | 2940 | 2301 | 2630 | 1254 | 8795 | 8808 | 8801 | 6407 |
| 7 | 2384 | 1606 | 2026 | 897 | 8601 | 8606 | 8603 | 6026 |
| 8 | 2047 | 1181 | 1604 | 791 | 8412 | 8421 | 8412 | 5688 |
| 9 | 1791 | 987 | 1289 | 713 | 8220 | 8208 | 8210 | 5377 |
| 10 | 1604 | 888 | 1085 | 657 | 8047 | 8028 | 8020 | 5087 |
Observations:
- Strong locality: LRU has 42.3% fewer faults than FIFO at 8 frames ((2047 - 1181) / 2047) and 44.6% fewer at 10 frames [C]. Clock lies between: 21.6% fewer faults than FIFO at 8 frames, but 35.8% more than LRU (1604 vs 1181) [C]; at 10 frames 22.2% more than LRU, at 3 frames only 1.3% more [C]. LRU has 49.3% more faults than Optimal at 8 frames [C].
- Why: LRU and Clock keep recently used pages, which are the working set; FIFO throws out a hot page just because it was loaded long ago.
- Random access: FIFO, LRU and Clock are within 0.4% of each other at every frame count [C] (8412, 8421, 8412 at 8 frames), because no policy can predict random references. Check [C]: with F frames and 50 pages the chance of a miss is 1 - F/50, so 94% at 3 frames and 80% at 10 frames; measured 9421 and 8020 to 8047 of 10,000.
- With strong locality the faults fall fastest between 5 and 8 frames [M], around the working-set size of 6, then flatten. At 10 frames LRU has 888 faults; a rough estimate [C]: the 10% random references mostly miss, 0.1 x 10,000 x (1 - 10/50) = 800, plus 6 cold faults at each of the 10 phase starts = 60, so about 860, close to 888 [M].

## 7. Running time for 100,000 references - Table 5.4
8 frames [A], 5 runs, median in ms [M] (timing_out.txt):
| String | FIFO | LRU | Clock | Optimal (fast) | Optimal (naive, as in the paper) |
|---|---|---|---|---|---|
| Locality | 0.78 | 1.01 | 1.07 | 1.18 | 3.14 |
| Random | 0.44 | 1.39 | 1.18 | 1.55 | 7.84 |
- All are very fast (a few ns per reference, for example LRU on locality 1.01 ms / 100,000 = 10.1 ns [C]). FIFO is fastest because a fault only advances a pointer.
- On the random string Clock is 15% faster than LRU ((1.39 - 1.18) / 1.39) [C]; on the locality string they are about the same (1.07 vs 1.01, runs overlap). In my simulation both Clock and LRU do the same lookup of the page in the frames; the real saving of Clock is that hardware sets one bit, instead of the OS or hardware updating a time stamp or a list at every access. That saving is not visible in a software simulation, so I do not claim it from my numbers [T].
- Optimal(naive) re-scans the future for each frame at every fault: 2.7 times slower than the fast version on locality (3.14 / 1.18) and 5.1 times on random (7.84 / 1.55) [C]. The fast version precomputes the next use of every reference once (extra array of n ints) and gives the same faults (check: "optimal_fast == optimal_naive: YES").
Complexity [T]: n references, F frames. My FIFO, LRU and Clock search the frames at every reference: time O(n x F), space O(F). Optimal naive: O(n x F x n) worst case; fast version O(n x F) time with O(n) extra space.

## 8. Alternative: Clock instead of exact LRU
- Solution: add a reference bit per frame and a hand (clock in experiment.c). On a hit set bit = 1; on a fault advance the hand, clearing bits, until a frame with bit 0 is found.
- Time per reference [T]: LRU must update a time stamp or move an entry in a list on every access (O(1) work but at every reference, and it needs the hardware to do it) and find the oldest frame on a fault (O(F) by scan, O(1) with a linked list); Clock sets one bit (done by the MMU, no OS work) and on a fault moves the hand, at most F steps (O(F) worst case, amortised O(1)).
- Extra space per frame [T]: LRU one time stamp (for example 8 bytes) or two list pointers; Clock one bit.
- Closeness to LRU in my simulation [C]: at 3 frames Clock has 1.3% more faults than LRU, at 6 frames 14.3% more (2630 vs 2301), at 8 frames 35.8% more, at 10 frames 22.2% more; on random access they are equal (within 0.4%). Clock is clearly better than FIFO (21.6% fewer faults at 8 frames) but not as good as LRU in this simulation.

## 9. Engineering decision - web server
Use an LRU-type policy; I recommend exact LRU for the server's own in-memory page cache and Clock for pages managed by the kernel.
- A web server has strong locality (a few popular pages receive most requests). On such a string LRU had 42% to 45% fewer faults than FIFO at 8 to 10 frames [M, C], and it was the best implementable policy at every frame count; FIFO is the worst.
- If the access pattern were random, the policy would not matter (all within 0.4% [M]), so the cheapest one (FIFO) would do; that does not describe a web server.
- Exact LRU in the application cache is cheap (a hash table plus a doubly linked list, O(1) per request), so its hardware problem does not arise there. For the OS's own page replacement Clock is the practical choice: it needs only the reference bit, and it gave 21.6% fewer faults than FIFO at 8 frames [C], although 35.8% more than LRU [C]. Optimal (the lower bound, 791 faults at 8 frames) cannot be built.

## 10. Limits
- The strings are synthetic (assumed 50 pages, working set 6, 90% locality); a real server trace would give different numbers.
- Timings are a software simulation on WSL2 (one machine, 5 runs, median); they do not measure the hardware cost of LRU.
- Belady's anomaly appeared only on the special string, not on my generated strings.
