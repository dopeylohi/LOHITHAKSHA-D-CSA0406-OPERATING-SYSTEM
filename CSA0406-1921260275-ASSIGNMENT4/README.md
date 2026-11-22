# CSA0406 Operating Systems – Assignment 4 (CO4: Optimising File Storage and Disk Access)

## Student details
- Name: Lohithaksha D
- Register No: 1921260275
- Department / Section: Information Technology / MS2 (Slot B)
- Year / Semester: 1st Year, 2nd Semester (UG)
- Course: CSA0406 Operating Systems
- Assignment: 4 (6 tasks, 50 marks)

## Environment
Ubuntu 26.04.1 on WSL2, gcc 15.2.0. The repository folder is on an ext4 volume (device `/dev/sdd`).

## Folder guide
| Folder | Task | Contents |
|--------|------|----------|
| `Task01/` | File-system design review (6 marks) | `task01.sh` (search-cost calculation and a modes-and-links model of the improved design), `defence.txt`, `notes.md`, `output.txt` |
| `Task02/` | Choosing an allocation method (8 marks) | `alloc.c` (Complete the Code: contiguous first-fit), `make_boundary.sh` (builds `boundary.c`), `calc.sh`, `fat_vs_index.c`, `notes.md`, `output_*.txt` |
| `Task03/` | Modern file systems (9 marks) | `task03.sh` (df, stat, ls -i, tune2fs on the real ext4), `tune2fs_full.txt`, `notes.md`, `output.txt` |
| `Task04/` | Disk scheduling (7 marks) | `sstf.c` (Complete the Code), `sim.c` (modes T, A, B, C), `sys.sh`, `sim_runs.csv`, `notes.md`, `output_*.txt` |
| `Task05/` | Distributed and cloud storage, device management (7 marks) | `calc5.sh`, `dev.sh`, `notes.md`, `output_*.txt` |
| `Task06/` | Storage design for a video-streaming start-up (13 marks) | `sim6.c`, `calc6.sh`, `notes.md`, `output_*.txt` |

Each `notes.md` holds the written answers for that task, with the program outputs copied at the end. The Word report follows the question paper's task order and contains the terminal screenshots.

## How to build and run
Run each task from its own folder (`cd CSA0406-1921260275-ASSIGNMENT4/TaskNN`).

~~~
# Task 1
bash task01.sh

# Task 2
gcc -std=c11 -Wall -Wextra -o alloc alloc.c && ./alloc
bash make_boundary.sh && gcc -std=c11 -Wall -Wextra -o boundary boundary.c && ./boundary
bash calc.sh
gcc -std=c11 -O1 -Wall -Wextra -o fat_vs_index fat_vs_index.c && ./fat_vs_index

# Task 3 (needs sudo for tune2fs; the folder must be on ext4)
sudo -v
bash task03.sh

# Task 4
gcc -std=c11 -Wall -Wextra -o sstf sstf.c && ./sstf
gcc -std=c11 -O2 -Wall -Wextra -o sim sim.c -lm
./sim T      # 4.1 working
./sim A      # 4.2 simulation, 10,000 requests, 3 loads, median of 5 seeds
./sim B      # 4.3 starvation of cylinder 183 under a stream near cylinder 60
./sim C      # 4.3 mean wait by cylinder zone
bash sys.sh  # 4.4 scheduler of the real disks

# Task 5
bash calc5.sh
bash dev.sh

# Task 6
gcc -std=c11 -O2 -Wall -Wextra -o sim6 sim6.c -lm && ./sim6
bash calc6.sh
~~~

## Key results (labels: [M] measured or simulated, [C] calculated, [T] typical published value, [A] assumed)
| Task | Result |
|------|--------|
| 1 | Flat directory of 12,000 students x 50 files: 300,000 comparisons on average; improved tree: 158; ratio about 1,898 [C], with 50 files per student, 12 departments and 4 batches assumed [A] |
| 2 | `alloc.c` output matches the expected output exactly [M]. Contiguous E of 14 blocks fails although 21 blocks are free; indexed E of 20 blocks succeeds [M]. Last block of a 4 GB file: contiguous 1 read, linked 1,048,576, FAT 1 disk read + 1,048,575 memory look-ups, indexed 3 [C]. Bit vector for a 4 TiB disk: 128 MiB [C]. FAT look-up to block 1,048,575 takes a median of about 16.8 ms against about 1.8 ns for the index (median of 5 runs, memory only) [M] |
| 3 | Real file system: ext4, block size 4096, journaling mode `data=ordered`, a 5000-byte file uses 16 sectors of 512 bytes (2 blocks), a hard link shares one inode [M]. The comparison table values are published values [T] |
| 4 | Head movement for queue 98 183 37 122 14 124 65 67 from 53: FCFS 640, SSTF 236, SCAN 236, C-SCAN 183 (+199 jump), LOOK 208, C-LOOK 153 (+169 jump) [C]. At heavy load FCFS average wait is about 23,250 ms while the other five stay between 33 and 54 ms [M, simulation]. Under SSTF the request at 183 waits 30,039.5 ms behind a stream of 10,000 requests [M, simulation]. The disks on this machine show scheduler `none` [M] |
| 5 | 100 MB file, 1000 users: single NFS server 80 s, 10-node cluster 8 s for reads [C, assumed links]. Three independent copies lose a block with probability 0.000001 against 0.01 for one copy [C, p = 0.01 assumed] |
| 6 | Peak 17,857 concurrent streams = 4,960 MB/s [C]. Hard disks only: 6 servers; SSD hot tier + hard disks: 3 servers [C]. At 4,500 streams per server the start-up time p95 is 4,066.3 ms for hard disks only and 55.1 ms with the SSD tier [M, simulation] |

## Assumptions and deviations
- **Repository:** the paper asks for a repository named `CSA0406-<RegNo>-Assignment4`. This assignment is stored as the folder `CSA0406-1921260275-ASSIGNMENT4` inside the existing repository `LOHITHAKSHA-D-CSA0406-OPERATING-SYSTEM`, next to the earlier assignments.
- **Screenshots:** the terminal screenshots are in the Word report, not in the repository. The text of every output is in the `output_*.txt` files and at the end of each `notes.md`.
- **Task 2:** the question says E (10 blocks) cannot be stored contiguously. With the given layout it fits at block 19 (as the expected output shows); the statement is true for E larger than 13 blocks, which `boundary.c` demonstrates.
- **Tasks 4 and 6 are simulations.** Seek, rotation and transfer times, load levels, popularity and prices are assumptions [A] stated in each `notes.md`. They show the shape of the differences, not real hardware speeds. Random seeds are fixed, so the results are repeatable.
- **Timing and simulations** were repeated 5 times (5 seeds) and the median is reported. Raw per-seed values for Task 4 are in `Task04/sim_runs.csv`.
- **Task 3:** no copy-on-write file system was available, so the Btrfs/ZFS alternative is reasoning with typical values [T], not measurements.
- **Task 5:** all numbers are calculations on stated assumptions. **Task 6:** `fio` and a real CDN were not used.
- The loop-and-pipe style `tee` commands were used to capture the real outputs; nothing was edited by hand.
