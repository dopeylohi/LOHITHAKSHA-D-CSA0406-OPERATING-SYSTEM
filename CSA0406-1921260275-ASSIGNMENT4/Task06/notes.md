# Task 6 – Storage Design for a Video-Streaming Start-up (13 marks)

200,000 students; videos of 300 MB to 2 GB, uploaded once, read thousands of times, mostly in the first week; students jump to any minute; a few servers with hard disks and SSDs, cloud allowed.

Labels: [M] measured (here: simulated, see Stage 5), [C] calculated (`output_calc6.txt`), [T] typical published value, [A] assumed. Every [A] value is listed where it is used.

## Stage 1 – Interpret the workload (2 marks)

### Operations and access patterns
| Operation | Pattern | Frequency |
|-----------|---------|-----------|
| Upload a video | Sequential write of 0.3 to 2 GB, once | Rare |
| Play a video | Long sequential read at the video's bit rate | Dominant: about 17,857 streams at peak [C] |
| Jump to any minute | Random: read the container index, then the data at that offset | Every session start and every seek |
| Popularity | A few new videos get most reads in week 1 (skewed, modelled as Zipf, 1/rank [A]) | The top 10% of videos (200) receive about 72% of requests [C] |
| Update or delete | Whole file only | Very rare |

Assumptions [A]: 5 viewing hours per student per week, peak = 3 x the average, average video 1000 MB of 60 minutes, 2,000 videos. Peak concurrent viewers = 200,000 x 5 / 168 x 3 = 17,857 [C]. One stream = 1000 MB / 3600 s = 0.278 MB/s = 2.22 Mbit/s [C]. Total at peak = 4,960 MB/s = 39.7 Gbit/s [C]. Traffic per month = 4.33 PB [C].

### Requirements as testable statements
1. R1: for at least 95% of playback starts, at any start minute, the first chunk arrives within 2 s (measured as p95 start latency in the simulation).
2. R2: the system sustains 17,857 concurrent streams of 0.278 MB/s each (4,960 MB/s) with at most 0.1% of chunks later than 10 s [A threshold].
3. R3: after the failure of any one disk or one server, every video is still readable and no data is lost (at least 2 online copies plus 1 cloud copy).
4. R4: the 200 most popular videos are served from SSD or RAM, not from hard disks.
5. R5: a newly uploaded 2 GB video is playable within 10 minutes [A].
6. R6: at most 4 servers are needed (the "few servers" budget).

## Stage 2 – Storage architecture (2 marks)

| Layer | Choice | Reason |
|-------|--------|--------|
| Hot tier | NVMe SSD on each server, holds the 200 most popular videos (200 GB [C]) | Serves 72% of requests with about 1.4 ms per 4 MiB chunk and no seek time |
| Capacity tier | 12 hard disks per server, whole library with 2 replicas (4,000 GB [C]) on different disks and servers | Cheapest storage per GB; sequential reads suit hard disks |
| Cloud tier | One copy of every video in object storage (2,000 GB, about $46 per month [C, T price]) | Off-site backup and source for rebuilding; holds old, rarely watched videos |
| File system | XFS (ext4 is an acceptable alternative) | Designed for large files and parallel I/O, extent-based, fast to delete large files |
| Allocation method | Extent-based indexed allocation (inode with extents), file size preallocated at upload | From Task 2: indexed allocation gives direct access to any block in a few reads, and preallocation keeps a 1 GB file in a few long extents, so playback reads are sequential. Contiguous allocation would suffer external fragmentation as videos are added and removed; linked allocation needs O(k) reads to reach minute 59 |

Each video is one file with its index (such as the MP4 `moov` box) at the front, so one read gives the byte offset of any minute.

## Stage 3 – Critical design decisions (2 marks)

| Decision | Choice | Evidence |
|----------|--------|----------|
| Block size | File-system block 4 KiB; I/O and read-ahead request 4 MiB | With 8.2 ms positioning time [A], 4 KiB requests are 0% efficient, 1 MiB 46%, 4 MiB 77%, 16 MiB 93% [C]. 4 MiB gives about 418 streams per hard disk against 503 for 16 MiB, but needs 4 times less buffer memory per stream and starts playback faster. Internal fragmentation of 4 KiB blocks is at most 4 KiB per 1 GB file (negligible) |
| Disk scheduler | `mq-deadline` on hard disks; `none` on NVMe | Task 4: SSTF can starve far requests (the 183 request waited 30,039.5 ms behind a stream), while elevator algorithms are bounded. `mq-deadline` is an elevator with a deadline per request. Task 4.4: SSDs gain nothing from reordering |
| Caching | SSD hot tier for the top 10% of videos plus the RAM page cache; new uploads are copied to the SSD tier for their first week | Top 200 of 2,000 videos with Zipf popularity = 71.9% of requests [C]; measured 71.6 to 71.9% in the simulation [M] |
| Replication factor | 2 online copies plus 1 cloud copy | If a copy is lost with probability 0.01 [A, independent], 2 copies lose data with probability 0.0001 and 3 copies with 0.000001 [C]. The cloud copy gives the third, independent copy without 3 times the hard disks (6,000 GB instead of 4,000 GB [C]) |

## Stage 4 – Design and calculations (4 marks)

All working is in `output_calc6.txt`; the main results are:

1. **Capacity [C]:** library 2,000 videos x 1000 MB = 2,000 GB; hard disks with 2 replicas 4,000 GB; hot set on SSD 200 GB; cloud copy 2,000 GB.
2. **Throughput [C]:** 17,857 x 0.2778 MB/s = 4,960 MB/s at peak. A hard disk doing 4 MiB reads (8.2 ms + 28.0 ms) gives 27.7 reads/s = 418 streams. A server with 12 disks supports 5,011 streams from disk; its 25 Gbit/s link supports 11,250 streams; its NVMe (3000 MB/s) supports 10,800 streams.
3. **Servers needed [C], with 70% headroom:** design A (hard disks only): limit 5,011 streams, 3,507 with headroom, so ceil(17,857 / 3,507) = 6 servers. Design B (SSD tier + hard disks): 72% of reads leave the hard disks, so the network limit of 11,250 applies, 7,875 with headroom, so 3 servers. The capacity calculation ignores session starts; the simulation includes them.
4. **Disk reads to start playing at minute 59 of a 1000 MB video (4 KiB blocks) [C]:** block 240,071 of 244,141 lies in the double-indirect range of a classic inode (it starts at block 1,036), so a classic inode needs inode + container index + double-indirect + single-indirect + data = 5 reads (69.0 ms on a hard disk, 1.9 ms on SSD). An extent-based file needs inode + container index + data = 3 reads (52.6 ms on a hard disk, 1.7 ms on SSD), 4 if the extent list is too long for the inode. With the inode and index cached in RAM it is 1 read: 36.2 ms on a hard disk, 1.5 ms on SSD. All are far below the 2 s target in R1; queueing under load is what matters, and Stage 5 measures it.

## Stage 5 – Implement and demonstrate (3 marks)

`sim6.c` (C, `gcc -O2 ... -lm`) simulates one server for 120 s, with these assumptions [A]: 12 hard disks (150 MB/s, 4.2 ms rotational latency, seek 1 ms + 0.04 ms per cylinder, LOOK scheduling from Task 4), 2 replicas per video with the request sent to the copy with the shorter queue, a 3000 MB/s NVMe with 0.1 ms per read, 2,000 videos with Zipf popularity, one 4 MiB chunk request per stream every 15.1 s, 50 session starts or jumps per second, each start costing 2 extra metadata reads of 8.2 ms. A chunk later than 10 s counts as a stall. Each design was run with 5 random seeds and the median is reported.

- **Design A:** hard disks only.
- **Design B:** the 200 most popular videos are served from the SSD, everything else from the hard disks.

Results from `output_sim6.txt` [M, simulation]:

| Streams per server | Design | Start median | Start p95 | Start p99 | Late chunks | Hard disk busy | Served by SSD |
|--------------------|--------|--------------|-----------|-----------|-------------|----------------|---------------|
| 4000 | A: HDD only | 115.1 ms | 324.3 ms | 526.3 ms | 0.00% | 96.8% | 0% |
| 4000 | B: SSD tier + HDD | 2.0 ms | 54.9 ms | 74.3 ms | 0.00% | 28.3% | 71.9% |
| 4500 | A: HDD only | 1033.0 ms | 4066.3 ms | 5556.3 ms | 0.32% | 101.9% | 0% |
| 4500 | B: SSD tier + HDD | 2.2 ms | 55.1 ms | 75.7 ms | 0.00% | 31.2% | 71.6% |

Reading the results:
- At 4,000 streams design A just meets R1 (p95 324 ms) but its disks are 96.8% busy, so there is no headroom. At 4,500 streams the hard disks are overloaded (101.9% busy, a backlog grows), the start p95 is 4,066 ms and fails R1, and 0.32% of chunks are late, which fails the 0.1% limit of R2.
- Design B keeps p95 at about 55 ms at both loads and no late chunks, with the hard disks only 28 to 31% busy. About 72% of requests go to the SSD, matching the 71.9% calculated from the popularity model.
- Together with Stage 4 (3 servers for B against 6 for A) this supports design B.
- Limits: this models one server, the service times are assumed, the network is not modelled, and popularity is an assumed Zipf curve. The numbers show the shape of the difference, not real hardware speeds. No `fio` test was run.

### Choice
Design B: SSD hot tier + replicated hard disks (XFS, extent allocation, 4 MiB reads, `mq-deadline` on hard disks, 2 copies + 1 cloud copy). It meets R1 to R4 and R6 in the simulation and calculation, and needs half the servers of design A.

## Alternative: cloud object storage with a content-delivery network

**Solution.** Store all videos in object storage (S3 or Google Cloud Storage), put a CDN in front, give players signed URLs, and keep no video servers of our own. Players fetch chunks from the nearest CDN edge; a cache miss fetches the chunk once from the object store.

Assumptions [A]: origin in Chennai; edge about 15 ms from every city; 5% of requests miss at the edge; home link 20 Mbit/s; the first 2 seconds of video (0.556 MB) are needed to start.

**Time to start playback [C]** (3 round trips + first segment + read time; own server read 52.6 ms from Stage 4):

| City | Round trip to origin | Own servers | CDN edge |
|------|----------------------|-------------|----------|
| Chennai | 10 ms | 304.8 ms | 275.9 ms |
| Bengaluru | 25 ms | 349.8 ms | 275.9 ms |
| Mumbai | 45 ms | 409.8 ms | 275.9 ms |
| Kolkata | 60 ms | 454.8 ms | 275.9 ms |
| Delhi | 70 ms | 484.8 ms | 275.9 ms |

The CDN gives the same start time everywhere and saves up to about 209 ms in the farthest city; both are far below the 2 s target, so latency is not the deciding factor. [A: not measured; no CDN was tested.]

**Cost per month [C]** with 4,330,000 GB (4.33 PB) transferred: object storage 2,000 GB x $0.023 = $46 [T price]. CDN transfer at an assumed blended price of $0.02, $0.03 or $0.05 per GB is $86,600, $129,900 or $216,500. Own servers pay for internet transit instead: at an assumed $0.005 or $0.01 per GB that is $21,650 or $43,300, plus the cost of 3 servers and staff. Transfer, not storage, is the dominant cost, and the result depends entirely on real price quotes.

**Effort.** CDN: no servers, disks, replication or capacity planning to run; work is integration (signed URLs, cache rules), cost monitoring and reliance on one provider. Own servers: the company must buy, patch, monitor and replace hard disks, manage replication and the SSD tier, and size the network for 39.7 Gbit/s at peak.

**Verdict.** The CDN is much easier to run and gives uniform start times, but with 4.33 PB per month the transfer bill is large and depends on a price I could only assume. Own servers (design B) with a cloud backup copy are the evidence-based choice for the primary system if transit is cheaper than the CDN price; a CDN is worth adding for students far from the data centre or if own bandwidth cannot reach 39.7 Gbit/s.

## Program outputs (copied from the runs)

### output_sim6.txt
~~~
~~~

### output_calc6.txt
~~~
=== Stage 1/4: workload (assumptions [A]: 5 viewing hours per student per week, peak = 3 x average, 1000 MB video of 60 min, 2000 videos) ===
Concurrent viewers at peak = 200000 x 5 / 168 x 3 = 17857
Bitrate of one stream      = 1000 MB / 3600 s = 0.2778 MB/s = 2.22 Mbit/s
Total read rate at peak    = 17857 x 0.2778 = 4960 MB/s = 39.7 Gbit/s
Traffic per month          = 200000 x 5 x 4.33 weeks x 1000 MB/hour = 4330000 GB = 4.33 PB

=== Stage 4: capacity ===
Library = 2000 videos x 1000 MB = 2000 GB
On HDD with 2 replicas = 4000 GB; hot set on SSD = 200 videos x 1000 MB = 200 GB; cloud copy = 2000 GB
With 3 replicas instead of 2: 6000 GB on HDD
Chance all copies are lost (p = 0.01 per copy [A], independent): 2 copies 0.000100, 3 copies 0.000001

=== Stage 3: request size on a hard disk (positioning 8.2 ms [A], 150 MB/s [A]) ===
size        transfer ms   efficiency    reads/s/HDD      streams/HDD
4       KiB         0.03           0%          121.5                2
64      KiB         0.44           5%          115.8               27
256     KiB         1.75          18%          100.5               95
1024    KiB         6.99          46%           65.8              249
4096    KiB        27.96          77%           27.7              418
16384   KiB       111.85          93%            8.3              503
Chosen: 4 MiB. One chunk = 15.1 s of video, so each stream needs one read every 15.1 s

=== Stage 4: servers needed (per server: 12 HDD, 25 Gbit/s link, one 3000 MB/s NVMe in design B; 70% headroom) ===
Hit ratio of the 200 most popular videos with Zipf popularity (1/rank) = 0.719
Streams per server: HDD side 5011, network 11250, NVMe 10800
Design A (HDD only):     limit 5011 streams, with headroom 3507, servers = ceil(17857 / 3507) = 6
Design B (SSD + HDD):    limit 11250 streams, with headroom 7875, servers = ceil(17857 / 7875) = 3

=== Stage 4: disk reads to start playing at minute 59 of a 60-minute, 1000 MB video (4 KiB blocks) ===
Blocks in file = 244141; block at minute 59 = 240071; double-indirect range starts at 1036
Classic inode (Task 2): inode 1 + container index 1 + double-indirect 1 + single-indirect 1 + data chunk 1 = 5 reads
Extent-based (XFS/ext4): inode 1 + container index 1 + data chunk 1 = 3 reads (4 if the extent list is too long for the inode)
Time on HDD  : 5 reads = 4 x 8.2 + (8.2 + 28.0) = 69.0 ms ; 3 reads = 2 x 8.2 + (8.2 + 28.0) = 52.6 ms
Time on SSD  : 5 reads = 4 x 0.1 + (0.1 + 1.4) = 1.9 ms ; 3 reads = 1.7 ms
If inode and container index are already cached in RAM: 1 read = 36.2 ms on HDD, 1.5 ms on SSD

=== Alternative: cloud object storage + CDN (assumptions [A]: origin in Chennai, home link 20 Mbit/s, first 2 s of video = 0.556 MB) ===
Download of the first segment = 0.556 MB / 2.5 MB/s = 222 ms
city         RTT origin   start, own server ms     start, CDN edge ms
Chennai           10 ms                  304.8                  275.9
Bengaluru         25 ms                  349.8                  275.9
Mumbai            45 ms                  409.8                  275.9
Kolkata           60 ms                  454.8                  275.9
Delhi             70 ms                  484.8                  275.9
Start time = 3 round trips (connection, TLS, request) + first segment + read time; own server read = 52.6 ms (3 reads on HDD, from above); CDN edge read 5 ms, 5% misses add 20 ms + 52.6 ms

=== Alternative: monthly cost (prices [A], blended after volume discounts; real quotes differ) ===
Object storage: 2000 GB x $0.023 = $46 per month [T price]
CDN transfer 4330000 GB: at $0.02/GB = $86600 ; $0.03/GB = $129900 ; $0.05/GB = $216500 per month
Own servers, internet transit 4330000 GB: at $0.005/GB = $21650 ; $0.01/GB = $43300 per month (plus the servers and staff)
~~~
