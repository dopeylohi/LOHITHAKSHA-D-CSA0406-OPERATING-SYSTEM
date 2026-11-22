# Task 4 – Disk Scheduling Algorithms (7 marks)

Disk with cylinders 0-199, head at 53, queue 98 183 37 122 14 124 65 67.

Labels: [M] measured or simulated (outputs at the end of this file), [C] calculated, [T] typical published value, [A] assumed.

## 4.1 Order of service and head movement (2 marks)

Direction conventions (as in the given code): SCAN and LOOK go towards cylinder 0 first; C-SCAN and C-LOOK go towards 199 first. The return jump of C-SCAN and C-LOOK is shown separately, and the total is given with and without it.

| Algorithm | Order of service | Head movement [C] | Working |
|-----------|------------------|-------------------|---------|
| FCFS | 98 183 37 122 14 124 65 67 | 640 | 45+85+146+85+108+110+59+2 |
| SSTF | 65 67 37 14 98 122 124 183 | 236 | 12+2+30+23+84+24+2+59 |
| SCAN | 37 14 (0) 65 67 98 122 124 183 | 236 | 16+23+14+65+2+31+24+2+59 |
| C-SCAN | 65 67 98 122 124 183 (199) jump (0) 14 37 | 183 without the jump, 382 with it | 12+2+31+24+2+59+16 = 146, then 14+23 = 37, jump 199 |
| LOOK | 37 14 65 67 98 122 124 183 | 208 | 16+23+51+2+31+24+2+59 |
| C-LOOK | 65 67 98 122 124 183 jump 14 37 | 153 without the jump, 322 with it | 12+2+31+24+2+59 = 130, jump 183 to 14 = 169, then 23 |

The full step-by-step distances are printed by `./sim T` (`output_T.txt`). SSTF turns out equal to SCAN (236) for this queue: SSTF zig-zags 67 to 37 to 14 and then back to 98, which costs about the same as the sweep to 0.

## 4.2 Simulation (2 marks)

Program `sim.c`, `./sim A`. Method and assumptions:
- 10,000 requests, cylinders uniformly random 0-199, arrivals with exponentially distributed gaps [A]; start head at 100.
- Service time = 3.0 ms fixed (rotation and transfer) + 0.1 ms per cylinder moved [A].
- Load: light = mean gap 40 ms, medium = 12 ms, heavy = 5 ms [A].
- Waiting time = time from arrival until the head starts moving towards the request. A SCAN trip to the edge or a C-SCAN/C-LOOK return jump is counted as waiting for the request picked afterwards and as head movement.
- SCAN only travels to the edge when requests are waiting behind the head; C-SCAN always returns through 199 and jumps to 0, counting 199 cylinders for the jump [A].
- The whole simulation was repeated with 5 different random seeds; the table reports the median of the 5 values. The raw values are in `sim_runs.csv`.

Reading the results (numbers from `output_A.txt` at the end of this file):
- FCFS moves about 66 cylinders per request at every load. This matches the expected 200/3 = 66.7 for two independent uniform positions [C].
- Heavy load: FCFS average service time is about 3 + 0.1 x 66 = 9.6 ms [C], but a request arrives every 5 ms, so the queue grows without bound: average wait about 23,000 ms and maximum about 46,000 ms [M]. The other five algorithms keep the movement near 19.7 cylinders per request because the queue is long, so scheduling matters most at heavy load.
- Light load: SCAN (about 123 cylinders) and C-SCAN (about 174) move far more than FCFS, SSTF and LOOK (about 65), because with one request waiting they still ride to the edge. LOOK and C-LOOK avoid this.
- SSTF and LOOK have the lowest average wait at medium and heavy load. SSTF has the worst tail at heavy load: maximum wait about 8.6 times its average, against about 4.6 for SCAN and 3.4 for C-SCAN [C from the table].
- The sd/avg column (spread of waiting relative to the average) is lower for C-SCAN than SCAN at all three loads (0.81 against 1.14, 0.63 against 0.79, 0.62 against 0.76), and lower for C-LOOK than LOOK at all three loads. The price is a higher average wait for C-SCAN (about 54 ms against 37 ms at heavy load).

## 4.3 Fairness (2 marks)

### Which algorithms can starve a request
- **SSTF can starve.** It always serves the nearest request, so while new requests keep arriving near the head, a far request is never the nearest. Measured with `./sim B` (stream of one request per millisecond near cylinder 60): the request at 183 waits 339.5 ms, 3039.5 ms and 30039.5 ms for streams of 100, 1000 and 10000 requests. That is about 3.0 ms x stream length + 39.5 ms [C], so the wait grows without limit as the stream continues, and 183 is served only after the whole stream (10,007 requests before it).
- **FCFS cannot starve** (bounded by the queue in front of it, 7.5 ms here) but it has the worst throughput.
- **SCAN, LOOK, C-SCAN, C-LOOK cannot starve in the unbounded sense.** The head must reach every cylinder in each sweep, so a request waits at most about one sweep plus the backlog. In the experiment the wait of 183 under SCAN levels off (305.7, 1298.7, 1709.7 ms) and under LOOK (287.9, 1148.9, 1334.9 ms) instead of growing with the stream. C-SCAN and C-LOOK served 183 after 22.1 ms only because they start towards higher cylinders and reached 183 in the first sweep before the stream built up; that is the start direction, not a special protection.

### Why C-SCAN waits are more uniform than SCAN
SCAN goes up and down, so a cylinder x (sweep length L) is visited at alternating gaps of 2x and 2(L-x). For a request arriving at a random time, the expected wait is (x² + (L-x)²) / L [C]. This is L at the edges and L/2 in the middle, so edge cylinders wait about twice as long as the middle. C-SCAN visits every cylinder once per cycle, with the same gap everywhere, so the expected wait is the same for all cylinders.

Measured with `./sim C` (mean wait by zone): at heavy load SCAN waits 40.9 / 29.1 / 40.9 ms (low / middle / high zone; middle/edges ratio 0.71) and C-SCAN waits 55.8 / 52.4 / 52.1 ms (ratio 0.97) [M]. The effect is weak at light and medium load, where few requests are queued; at medium load C-SCAN's zones even differ more (39.0 against 26.7 ms), so the uniformity claim is supported mainly at heavy load and by the lower sd/avg in 4.2.

## 4.4 Solid-state drives (1 mark)

An SSD has no moving head, so there is no seek distance to reduce and reordering requests by cylinder gains nothing. An elevator algorithm would only add CPU work and delay. An NVMe SSD also has many internal channels and multiple hardware queues, so the best strategy is to hand requests to the device quickly and let it parallelise. Therefore:
- `none` passes requests straight to the device (best for fast NVMe drives with their own multi-queue scheduling).
- `mq-deadline` keeps only request merging and a deadline per request (reads are served before writes, with a deadline for each [T]), so no request waits forever. It suits SATA SSDs and virtual disks.

The scheduler of my disk is in `output_sys.txt`: the name in square brackets is the active one, and the `rotational` flag says whether the kernel treats the device as a spinning disk. On WSL2 the disks are virtual (the values come from the virtual block device, not from real hardware).

## Complete the Code

`sstf.c` is the given program with TODO 1 completed: for each step it scans the requests not yet done, picks the one with the smallest `abs(req[i] - pos)`, marks it done, stores it in `order[k]` and moves `pos`. Output is in `output_sstf.txt` and matches the expected output exactly. The starvation experiment for the stream near cylinder 60 is `./sim B` (see 4.3).

## Alternative: LOOK and C-LOOK instead of SCAN and C-SCAN

**Solution.** The head turns at the last pending request in its direction instead of at the end of the disk.

~~~
LOOK(head, dir, queue):
    ahead = requests on the dir side of head
    if ahead is empty:  dir = -dir;  ahead = requests on the new side
    serve the nearest request in ahead;  head = its cylinder

C-LOOK: always dir = up; if nothing is ahead, jump to the lowest pending request and continue up
~~~

- Total head movement for the given queue [C]: LOOK 208 against SCAN 236 (saves 28 = 2 x 14, the trip from 14 to 0 and back); C-LOOK 153 against C-SCAN 183 without the jump (saves 30), and 322 against 382 with the jump counted (saves 60).
- Average and maximum waiting time [M] (from `output_A.txt`): LOOK has lower average wait than SCAN at all three loads, and C-LOOK lower than C-SCAN at all three loads, and the maximum wait is lower in five of the six comparisons and about equal in the sixth (heavy load: LOOK 169.1 ms against SCAN 168.8 ms). The differences are largest at light and medium load, where SCAN and C-SCAN waste trips to the edge; at heavy load there are always requests near the edges, so the difference is small.
- Extra work: LOOK and C-LOOK need the position of the last pending request. With an unsorted queue this is an O(n) scan of the queue each time the head must decide whether to turn. With a queue kept sorted by cylinder (balanced tree, insertion O(log n)) the nearest request ahead and the last request are found in O(log n) or O(1). SCAN and C-SCAN need no search, because the end of the disk is a constant. In my simulator the scan is O(n) per request, which is fine for 10,000 requests.

## Program outputs (copied from the runs)

### output_sstf.txt
~~~
FCFS   53 98 183 37 122 14 124 65 67  -> 640 cylinders
SSTF   53 65 67 37 14 98 122 124 183  -> 236 cylinders
SCAN   53 37 14 0 65 67 98 122 124 183  -> 236 cylinders
C-SCAN 53 ... 199, jump to 0 ... 37  -> 183 cylinders (+199 for the jump)
~~~

### output_T.txt
~~~
Head 53, queue 98 183 37 122 14 124 65 67, cylinders 0-199.
SCAN and LOOK go towards 0 first; C-SCAN and C-LOOK go towards 199 first (as in the given code).

FCFS   order: 98 183 37 122 14 124 65 67
  |53-98|=45 + |98-183|=85 + |183-37|=146 + |37-122|=85 + |122-14|=108 + |14-124|=110 + |124-65|=59 + |65-67|=2 = 640

SSTF   order: 65 67 37 14 98 122 124 183
  |53-65|=12 + |65-67|=2 + |67-37|=30 + |37-14|=23 + |14-98|=84 + |98-122|=24 + |122-124|=2 + |124-183|=59 = 236

SCAN   order: 37 14 (0) 65 67 98 122 124 183
  |53-37|=16 + |37-14|=23 + |14-0|=14 + |0-65|=65 + |65-67|=2 + |67-98|=31 + |98-122|=24 + |122-124|=2 + |124-183|=59 = 236

C-SCAN order: 65 67 98 122 124 183 (199) jump (0) 14 37
  |53-65|=12 + |65-67|=2 + |67-98|=31 + |98-122|=24 + |122-124|=2 + |124-183|=59 + |183-199|=16 = 146
  jump 199 -> 0 = 199 (counted separately)
  |0-14|=14 + |14-37|=23 = 37
  total without jump = 183, with jump = 382

LOOK   order: 37 14 65 67 98 122 124 183
  |53-37|=16 + |37-14|=23 + |14-65|=51 + |65-67|=2 + |67-98|=31 + |98-122|=24 + |122-124|=2 + |124-183|=59 = 208

C-LOOK order: 65 67 98 122 124 183 jump 14 37
  |53-65|=12 + |65-67|=2 + |67-98|=31 + |98-122|=24 + |122-124|=2 + |124-183|=59 = 130
  jump 183 -> 14 = 169 (counted separately)
  |14-37|=23 = 23
  total without jump = 153, with jump = 322
~~~

### output_A.txt
~~~
10000 random requests, cylinders 0-199, start head 100. Median of 5 seeds. [A] seek 0.1 ms/cyl + 3.0 ms fixed.
Head movement includes the SCAN trip to the edge and the C-SCAN/C-LOOK return jump.

light load: mean inter-arrival 40 ms
alg         avg move    avg wait ms    max wait ms     sd wait ms    sd/avg
FCFS           66.23            1.9           49.3            4.7      2.45
SSTF           65.11            1.7           45.5            4.2      2.43
SCAN          122.84            8.1           56.1            9.3      1.14
C-SCAN        173.61           22.0           89.3           17.8      0.81
LOOK           65.13            1.8           44.9            4.3      2.42
C-LOOK         65.85            5.1           54.2            6.6      1.28

medium load: mean inter-arrival 12 ms
alg         avg move    avg wait ms    max wait ms     sd wait ms    sd/avg
FCFS           66.23           24.0          175.9           27.0      1.13
SSTF           55.23            8.1           98.9           10.4      1.28
SCAN           74.39           18.3           82.1           14.5      0.79
C-SCAN         82.19           31.8          115.0           20.0      0.63
LOOK           55.40            8.4           69.1           10.1      1.19
C-LOOK         58.72           13.9           81.8           13.3      0.95

heavy load: mean inter-arrival 5 ms
alg         avg move    avg wait ms    max wait ms     sd wait ms    sd/avg
FCFS           66.23        23249.5        46456.7        13482.6      0.58
SSTF           19.59           33.9          290.7           32.3      0.95
SCAN           19.69           36.7          168.8           28.0      0.76
C-SCAN         19.75           53.7          183.9           33.1      0.62
LOOK           19.57           32.8          169.1           25.8      0.79
C-LOOK         19.71           47.4          174.1           30.8      0.65

Raw per-seed values are in sim_runs.csv
~~~

### output_B.txt
~~~
Head at 53, queue 98 183 37 122 14 124 65 67 at t=0, then one request every 1 ms near cylinder 60 (58..62 repeating).
Tracked request: cylinder 183. [A] seek 0.1 ms/cyl + 3.0 ms fixed.

Stream of 100 requests (lasts 100 ms)
alg         wait of 183 (ms) requests served before it
FCFS                     7.5                        1
SSTF                   339.5                      107
SCAN                   305.7                       96
C-SCAN                  22.1                        5
LOOK                   287.9                       91
C-LOOK                  22.1                        5

Stream of 1000 requests (lasts 1000 ms)
alg         wait of 183 (ms) requests served before it
FCFS                     7.5                        1
SSTF                  3039.5                     1007
SCAN                  1298.7                      427
C-SCAN                  22.1                        5
LOOK                  1148.9                      378
C-LOOK                  22.1                        5

Stream of 10000 requests (lasts 10000 ms)
alg         wait of 183 (ms) requests served before it
FCFS                     7.5                        1
SSTF                 30039.5                    10007
SCAN                  1709.7                      564
C-SCAN                  22.1                        5
LOOK                  1334.9                      440
C-LOOK                  22.1                        5
~~~

### output_C.txt
~~~
Mean wait (ms) by cylinder zone, pooled over 5 seeds x 10000 requests.
Zones: low = cylinders 0-66, middle = 67-132, high = 133-199.

light load
alg            low     middle       high   middle/edges
SCAN           8.2        7.9        8.3           0.96
C-SCAN        32.1       20.9       13.2           0.92
LOOK           1.8        1.7        1.8           0.96
C-LOOK         8.9        4.4        2.2           0.80

medium load
alg            low     middle       high   middle/edges
SCAN          19.3       16.2       19.3           0.84
C-SCAN        39.0       29.7       26.7           0.90
LOOK           9.0        7.4        9.0           0.82
C-LOOK        17.8       12.8       11.5           0.87

heavy load
alg            low     middle       high   middle/edges
SCAN          40.9       29.1       40.9           0.71
C-SCAN        55.8       52.4       52.1           0.97
LOOK          36.1       25.6       36.4           0.71
C-LOOK        49.6       46.4       46.6           0.96
~~~

### output_sys.txt
~~~
/sys/block/sda: scheduler=[none] mq-deadline kyber   rotational=1
/sys/block/sdb: scheduler=[none] mq-deadline kyber   rotational=1
/sys/block/sdc: scheduler=[none] mq-deadline kyber   rotational=1
/sys/block/sdd: scheduler=[none] mq-deadline kyber   rotational=1

NAME    SIZE ROTA TYPE
ram0      4M    0 disk
ram1      4M    0 disk
ram2      4M    0 disk
ram3      4M    0 disk
ram4      4M    0 disk
ram5      4M    0 disk
ram6      4M    0 disk
ram7      4M    0 disk
ram8      4M    0 disk
ram9      4M    0 disk
ram10     4M    0 disk
ram11     4M    0 disk
ram12     4M    0 disk
ram13     4M    0 disk
ram14     4M    0 disk
ram15     4M    0 disk
sda   722.9M    1 disk
sdb    97.4M    1 disk
sdc       4G    1 disk
sdd       1T    1 disk
~~~
