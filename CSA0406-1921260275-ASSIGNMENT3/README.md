# CSA0406 Operating Systems - Assignment 3 (CO3: memory management)

| | |
|---|---|
| Student | Lohithaksha D |
| Register number | 1921260275 |
| Department / section | Information Technology / MS2 (Slot B) |
| Academic year / semester | 1st year, 2nd semester (Sep-Dec 2026, UG) |
| Course | CSA0406 Operating Systems, Assignment 3 |
| Faculty | Jayasakthi Velmurugan Kannaiyan |
| Repository | https://github.com/dopeylohi/LOHITHAKSHA-D-CSA0406-OPERATING-SYSTEM (folder CSA0406-1921260275-ASSIGNMENT3) |

## Environment
- Linux: Ubuntu 26.04.1 on WSL2 (kernel 6.18 microsoft-standard-WSL2), 8939 MB RAM, 4 GB swap on /dev/sdc.
- Compiler: gcc 15.2.0. Compile command used for every C program: `gcc -std=c11 -Wall -Wextra -O2 -o <name> <name>.c` (Task01 adds `-static` for the static build and `-fPIC -shared` for the library; no thread library was needed, so no `-pthread`).
- Tools: strace 6.19, awk, bash, /usr/bin/time. Task05 and Task04 have an optional plot script (matplotlib).

## Folder guide
Each TaskNN folder holds the C source, shell scripts, outputs (`*.txt`, `*.csv`) and `notes.md` with the written answers, calculations and labelled results. Binaries and screenshots are not committed.
| Folder | Question | Main files |
|---|---|---|
| Task01 | Linking, loading, address binding | bankenc.c, app.c, dl_app.c, build.sh, bench.sh, share.sh, replace.sh, missing.sh |
| Task02 | Paging | paging.c (completed TODO 1), edge_test.c, frag.c, touch.c, calc.sh |
| Task03 | Segmentation | seg.c, maps_demo.c, segv.c, frag_seg.c, calc.sh, notes_theory.md |
| Task04 | Virtual memory and demand paging | touch2.c, memgrow.c, cow.c, fault_curve.c, calc.sh, run_limit.sh, plot4.py |
| Task05 | Page replacement | lru_complete.c (completed TODO 1), experiment.c, plot.py |
| Task06 | Disk management | raid.sh, disk_out.txt |
| Task07 | Memory plan for the edge device | rt_loop.c, hog.c, simulate.c, rt_test.sh |

## How to build and run
Set the folder variable: `A3=~/LOHITHAKSHA-D-CSA0406-OPERATING-SYSTEM/CSA0406-1921260275-ASSIGNMENT3`. Then `cd $A3/TaskNN && ./build.sh` (Task06 needs no build).
- Task01: `./app_static`, `./app_dyn`, `./dl_app none`, `./dl_app enc`, `./bench.sh`, `./share.sh`, `./replace.sh`, `./missing.sh`, `ldd app_dyn`.
- Task02: `./paging`, `./paging_2048`, `./edge_test`, `./frag`, `./calc.sh`, `/usr/bin/time -v ./touch 64`, `getconf PAGESIZE`.
- Task03: `./seg`, `./maps_demo`, `./segv null h`, `./segv ro h`, `./segv null`, `./frag_seg`, `./calc.sh`.
- Task04: `./touch2 seq 64`, `./touch2 rand 64`, `./cow cow 64`, `./cow time`, `./calc.sh`, `./fault_curve > fault_curve.csv`, `./run_limit.sh 256` (needs sudo and systemd; takes 5 to 20 minutes).
- Task05: `./lru_complete`, `./experiment trace`, `./experiment check`, `./experiment csv > results.csv`, `./experiment timing`, `python3 plot.py`.
- Task06: `./raid.sh`; `lsblk`, `sudo fdisk -l`, `swapon --show`, `free -m`, `findmnt /` (saved in disk_out.txt).
- Task07: `./simulate`, `./rt_test.sh` (needs sudo; about 6 to 10 minutes).

## Key results (every number is in the matching notes.md with its working)
Labels: [M] measured, [C] calculated, [T] typical or textbook value, [A] assumed.
| Task | Result |
|---|---|
| 1 | app_static 821,680 B against app_dyn 16,528 B + libbankenc.so 15,224 B [M]; start-up median 705 us static, 907 us dynamic, +28.7% [C]; resident memory 720 kB against 1640 kB [M]; replacing only the library changed the output with an identical executable (same md5) [M]; missing library: exit code 127 for the linked program, exit code 2 with a clear message for dlopen [M]; libc code pages shared between two copies: Shared_Clean 1248 kB, Pss 52 kB [M] |
| 2 | Completed TODO 1 reproduces the expected output; PAGE_SIZE 2048 worked by hand and confirmed [C, M]; page table 4 MiB (32-bit) and 512 GiB (48-bit) [C]; EAT 130, 115, 111 ns at 80, 95, 99% hits (m = 100 ns, TLB 10 ns) [A, C]; mean internal fragmentation 514.9 B, 2085.7 B, 1,063,547.7 B per process for 1 KB, 4 KB, 2 MB pages [M]; touching 64 MB gave 16,384 minor faults [M] |
| 3 | 8 translations: 5 valid, 2 limit traps, 1 invalid segment [M]; NULL write: SEGV_MAPERR, constant write: SEGV_ACCERR, exit code 139 [M]; mean external fragmentation 43.9% (20 segments, 5 seeds) [M]; table sizes 24 B, 196 B, 12,288 B, 4,194,304 B [C] |
| 4 | EAT 8099.9, 179.999, 107.9999 ns for p = 1/1,000, 1/100,000, 1/1,000,000 [C]; 10% slow-down needs p <= 1.25e-6 [C]; copy-on-write: child Pss 32,924 kB against 65,979 kB before fork, 16,384 copy faults [M]; limited-memory runs: 21 of 36 killed, 15 finished, a 512 MB run took 33.3 s per pass against 2.97 ms without a limit [M]; column-major order 16,384 faults against 64 for row-major below 64 frames [M] |
| 5 | FIFO 15/10, LRU 12/8, Optimal 9/8 and Belady 9 to 10 reproduce the expected output [M]; with strong locality LRU has 42.3% fewer faults than FIFO at 8 frames [C]; random access: all within 0.4% [C]; 100,000 references take 0.44 to 1.55 ms (naive Optimal up to 7.84 ms) [M] |
| 6 | Usable capacity of 6 x 4 TB: RAID 0 24, RAID 1 pairs 12, RAID 5 20, RAID 6 16, RAID 10 12 TB [C]; yearly loss: RAID 5 1 in 31,977, RAID 10 1 in 159,867, RAID 6 1 in 291,788,676 [A, C]; RAID 6 recommended; SSD/HDD read speed not measured (only an SSD) |
| 7 | With forced reclaim the worst-case delay was 20.155 ms unlocked and 1.059 ms locked (median of 5), 232 deadline misses against 0 [M]; simulator: locked real-time task 0 faults, unlocked 1,440 when a fourth 500 MB task is added [M] |

## Assumptions
Page-table entries of 4 and 8 bytes; memory time 100 ns and TLB 10 ns; random process sizes up to 10 MB; segment sizes 4 to 64 KB; failure rate 2% a year, 24 h rebuild window, 64 GB server RAM; edge device: 300 MB OS reserve, 16 MB real-time task, 64 MB logging, 50 ms period and 10 ms deadline. All of these are marked [A] in the notes.

## Deviations and honest notes
- One repository for all assignments, with this folder inside it, instead of a new repository named CSA0406-1921260275-Assignment3.
- Screenshots are kept for the report and are not committed.
- Task04: the memory limit (cgroup) killed most runs at 256 MB and above; the failures are reported, not hidden, and no timing is claimed for them.
- Task07: the real memory hog did not evict the task's pages; the 20 ms result comes from forced reclaim (madvise PAGEOUT). rt_out.txt was overwritten by an accidental second invocation and re-typed from the terminal screenshot of the original complete run.
- Task06: no read-speed comparison (only an SSD); the RAID figures are calculations.
- Hand calculations (Task02 page size 2048, Task03 translations, Task05 hand trace) are written out in the notes and were checked against the program output.
