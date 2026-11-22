# Task 2 – From Workload to File-Allocation Method (8 marks)

Storage for a video-surveillance recorder: 32 cameras, files grow while recording, mostly read start to end, sometimes searched by time, deleted after 30 days.

Labels: [M] measured, [C] calculated (working in `output_calc.txt`), [T] typical published value, [A] assumed. All outputs are copied at the end of this file.

## 2.1 Operations, frequencies and access pattern (2 marks)

Assumptions [A]: 4 Mbit/s per camera, one segment file per camera per hour, 4 KiB blocks, 30-day retention.

| Operation | How often | Access pattern |
|-----------|-----------|----------------|
| Append (recording) | Continuous: 500,000 B/s per camera, 16,000,000 B/s in total = 3,906 block writes per second [C] | Sequential, append only, 32 streams at the same time |
| Create a segment file | 32 x 24 = 768 per day [C] | One-off metadata update |
| Read for playback or export | Rare [A: tens of sessions per day] | Sequential, start to end |
| Search by time | Rare [A: a few seeks per session] | Direct (random): jump to block k of a segment |
| Delete | 768 per day once 30 days are full [C]; each segment is 1,800,000,000 bytes = 439,454 blocks [C] | Whole file at once, never partial |

Conclusion: the workload is write-dominated (thousands of appends per second against a handful of reads), the writes are sequential appends to files that keep growing, reads are sequential with occasional direct jumps, and deletes free very large files. Mapping a time to a byte offset needs an index of timestamps in the application; with a constant bit rate offset = time x 500,000 B/s.

Consistency note [C]: 30 days at this rate is 41,472,000,000,000 bytes (about 41.5 TB), more than one 4 TB disk (which lasts about 2.9 days). So the real system needs several disks; the 4 TB size is used only for the free-space calculation in 2.4.

## 2.2 Comparison of the allocation methods (2 marks)

| Criterion | Contiguous | Linked | Linked with FAT (table in memory) | Indexed (inode) |
|-----------|------------|--------|-----------------------------------|-----------------|
| Sequential access | Best: one seek, then adjacent blocks | Fine if blocks are close, but each block must be read to learn the next one, so no read-ahead | Good: next block is known from the FAT in memory | Good: index lists the blocks in order, read-ahead is easy |
| Direct access to block k | O(1): start + k | O(k) disk reads | O(k) memory look-ups + 1 disk read | O(1): at most 4 reads (inode with 3 indirect levels) |
| Growth | Poor: needs free space right after the file, or the file must be copied; final size must be known | Excellent: any free block | Excellent | Good: take any free block and add a pointer |
| Fragmentation | Severe external fragmentation (shown by the boundary test) | No external fragmentation; blocks may be scattered, which makes the disk seek | Same as linked | No external fragmentation; small overhead for index blocks |
| Reliability | A bad sector damages only itself; losing the directory entry loses start and length | One damaged block breaks the chain, everything after it is lost | Damage to the FAT loses the chains of all files (real FAT keeps two copies [T]) | Damaged inode loses one file's map; damaged indirect block loses only its range |

### Chosen method: indexed allocation (inode with extents), one file per camera per hour
- Files grow continuously, which rules out contiguous allocation (it cannot grow, and 32 streams writing at once would break each other's runs).
- Searching by time needs direct access, which rules out plain linked allocation (O(k) reads).
- FAT would keep one entry for every block of the disk in memory: 4 GiB for a 4 TiB disk (see 2.4 and the alternative below).
- Indexed allocation gives direct access in at most 4 reads, grows block by block, and a damaged index loses one file only.
- Hourly files of about 1.8 GB fit inside the double-indirect range of the inode (up to 4 GiB), so a seek costs 3 reads. Deleting a whole hourly file frees its blocks in one step.

## 2.3 Cost calculation (3 marks)

Assumptions [A]: 4 GB = 4 GiB = 2^32 bytes; the directory entry / inode is already in memory; 4 KiB blocks.

Blocks in the file [C] = 4,294,967,296 / 4,096 = 1,048,576. The last block is number 1,048,575 (counting from 0).

| Method | Disk reads to reach the last block [C] | Working |
|--------|-----------------------------------------|---------|
| Contiguous | 1 | address = start + 1,048,575 |
| Linked | 1,048,576 | follow the chain from block 0; with the 4-byte pointer inside each block only 4,092 bytes are data, so 1,049,602 blocks and reads |
| Linked with FAT | 1 disk read + 1,048,575 memory look-ups | chain is followed in the FAT in memory |
| Indexed (inode) | 3 | pointers per block = 4096 / 4 = 1,024; direct covers 0..11, single 12..1035, double 1036..1,049,611, so the last block is in the double-indirect range: double-indirect block + single-indirect block + data block (4 if the inode itself must be read) |

### Maximum file size of the inode [C]
- Direct: 12 x 4,096 = 49,152 bytes (48 KiB)
- Single: 1,024 x 4,096 = 4,194,304 bytes (4 MiB)
- Double: 1,024 x 1,024 x 4,096 = 4,294,967,296 bytes (4 GiB)
- Triple: 1,024 x 1,024 x 1,024 x 4,096 = 4,398,046,511,104 bytes (4 TiB)
- Total: 4,402,345,721,856 bytes = 4 TiB + 4 GiB + 4 MiB + 48 KiB (1,074,791,436 blocks)
- A 32-bit block number could address 2^32 x 4,096 = 16 TiB, so the pointer width is not the limit; the three-level index is.

## 2.4 Free-space management for a 4 TB disk (1 mark)

Assumption [A]: 4 TB = 4 TiB = 2^42 bytes, 4 KiB blocks, so 2^30 = 1,073,741,824 blocks [C].

| Method | Memory / space | Speed | Comment |
|--------|----------------|-------|---------|
| Bit vector | 1,073,741,824 bits = 134,217,728 bytes = 128 MiB [C] (116 MiB if 4 TB means 4 x 10^12 bytes) | Finding a run of n free blocks is a bit scan; easy to find contiguous space | Fixed size; must be kept in memory (or cached) |
| Linked free list | No table; 4 bytes inside each free block | Allocating n blocks takes n reads | Cheap in space, slow, cannot find a contiguous run |
| Grouping | A block holds 1,023 free addresses + 1 link; an empty disk needs 1,048,576 group blocks, stored inside the free space [C] | One read gives 1,023 free blocks | No extra memory |
| Counting | (start, count) pairs of 8 bytes: best case a few entries; worst case alternating used/free = 536,870,912 runs x 8 = 4 GiB [C] | Fast for large runs | Good for large extents such as video files, bad for fragmented disks |

For the recorder (huge files, mostly large free runs) a bit vector (128 MiB) or counting suits best.

## Complete the Code

`alloc.c` holds the given program with TODO 1 completed (first-fit scan: count consecutive free blocks, reset the count on a used block, mark and return the start when the count reaches `size`). I also corrected the pasted `││` to `||`. Nothing else was changed. Output is in `output_alloc.txt` and matches the expected output exactly.

Reading the output:
- After A (0-5), C (14-18) and the releases, the free runs are 6-13 (8 blocks) and 19-31 (13 blocks): 21 free blocks but the longest run is 13 [M].
- The question says E (10 blocks) cannot be stored contiguously. For this exact size it can: 10 is at most 13, so first fit puts it at block 19 [M], as the expected output shows. The statement is true for any E larger than 13 blocks.
- `boundary.c` (built by `make_boundary.sh` from the same functions) tests this [M]: contiguous E of 13 blocks fits (start 19); contiguous E of 14 blocks fails (-1) although 21 blocks are free. This is external fragmentation: the free space is split into runs of 8 and 13 and contiguous allocation needs one unbroken run.
- Indexed allocation does not need a run: any free blocks can be used, and the index block records where they are. E of 20 blocks succeeds (1 index block + 20 data blocks = all 21 free blocks). E of 21 blocks fails (-1) because it would need 22 blocks: the price of indexing is one block of overhead.
- In the program both E files use the letter E. In the last map the E blocks at 19-28 are the contiguous copy; the lowercase `e` at 6 is the index block of the indexed copy, whose data blocks are 7-13 and 29-31.

## Alternative: linked allocation with a FAT in memory

**Solution.** Each disk block has one FAT entry holding the number of the next block of the same file (0 = free, a special value = end of file). The directory stores only the first block.

~~~
read_block(file, k):
    b = file.first
    repeat k times:  b = FAT[b]       # memory look-ups
    return disk_read(b)                # one disk read

append_block(file):
    b = first entry with FAT[b] == 0
    FAT[file.last] = b;  FAT[b] = END;  file.last = b
~~~

| Measure | FAT | Indexed (inode) |
|---------|-----|-----------------|
| Time to reach block k | O(k) memory look-ups + 1 disk read | O(1) (3 levels at most, 1 to 4 reads) |
| Memory | One entry per disk block, always in memory: 2^30 x 4 B = 4 GiB for 4 TiB [C]; space is O(D) for D disk blocks | Only the inodes of open files; space is O(open files) |
| Reliability | FAT damage loses every file's chain, so the FAT must be stored twice | Inode damage loses one file |

Measured [M] with `fat_vs_index.c` (4 GiB file scattered over the disk, 5 runs each, median reported, warm cache, in-memory look-ups only; the disk read is not timed): the FAT time grows in proportion to k, while the index time stays at a few nanoseconds. The table is at the end of this file. Look-up counts: FAT k, index 1 to 3 reads.

Verdict: FAT is acceptable for small disks but for a 4 TiB recorder volume it needs 4 GiB of memory and linear seeks, so indexed allocation stays the choice.

## Program outputs (copied from the runs)

### output_alloc.txt
~~~
AAAAAA........CCCCC.............
E (10 blocks) contiguous: start 19
E (10 blocks) indexed: index block 6, data blocks 7 8 9 10 11 12 13 29 30 31
AAAAAAeEEEEEEECCCCCEEEEEEEEEEEEE
~~~

### output_boundary.txt
~~~
Start: AAAAAA........CCCCC.............
free blocks = 21, largest free run = 13

contiguous E size 13 ->  19   AAAAAA........CCCCCEEEEEEEEEEEEE
contiguous E size 14 ->  -1   AAAAAA........CCCCC.............
indexed    E size 20 ->   6   AAAAAAeEEEEEEECCCCCEEEEEEEEEEEEE
indexed    E size 21 ->  -1   AAAAAA........CCCCC.............
~~~

### output_calc.txt
~~~
=== 2.1 Workload numbers (assumptions [A]: 32 cameras, 4 Mbit/s each, 1-hour segment files) ===
Bytes per second per camera   = 4 x 1,000,000 / 8 = 500000
Bytes per second, all cameras = 500000 x 32 = 16000000
Block appends per second      = 16000000 / 4096 = 3906 (all cameras)
Segment size (1 hour)         = 500000 x 3600 = 1800000000 bytes = 439454 blocks
New segment files per day     = 32 x 24 = 768 (also 768 deletes per day once 30 days are full)
Storage for 30 days           = 16000000 x 86400 x 30 = 41472000000000 bytes (about 41.5 TB)
A 4 TB (4x10^12) disk lasts at 16000000 B/s: 250000 s (about 2.9 days)

=== 2.3 Cost to reach the last block of a 4 GB file (assumption [A]: 4 GB = 4 GiB, directory entry / inode already in memory) ===
Blocks in file = 4294967296 / 4096 = 1048576 ; last block number (from 0) = 1048575
Contiguous : address = start + 1048575, so 1 disk read
Linked     : must follow the chain from block 0, so 1048576 disk reads
Linked, with the 4-byte pointer stored inside each block: 4092 usable bytes, so 1049602 blocks and 1049602 disk reads
FAT        : 1048575 memory look-ups in the FAT + 1 disk read for the data block
Indexed    : pointers per block = 4096 / 4 = 1024
  direct covers blocks 0..11; single covers 12..1035; double covers 1036..1049611
  block 1048575 is in the double-indirect range: double-indirect block + single-indirect block + data block = 3 disk reads

=== 2.3 Maximum file size of the inode (12 direct, 1 single, 1 double, 1 triple, 4-byte addresses) ===
Direct : 12 x 4096                = 49152 bytes (48 KiB)
Single : 1024 x 4096              = 4194304 bytes (4 MiB)
Double : 1024 x 1024 x 4096       = 4294967296 bytes (4 GiB)
Triple : 1024 x 1024 x 1024 x 4096 = 4398046511104 bytes (4 TiB)
Total  : 4402345721856 bytes (4 TiB + 4 GiB + 4 MiB + 48 KiB)
Blocks : 12 + 1024 + 1048576 + 1073741824 = 1074791436
A 32-bit block number can address 2^32 blocks x 4096 = 17592186044416 bytes (16 TiB), so addressing is not the limit.

=== 2.4 Free-space management for a 4 TB disk (assumption [A]: 4 TB = 4 TiB = 2^42 bytes, 4 KiB blocks) ===
Blocks = 2^42 / 4096 = 1073741824
Bit vector = 1073741824 bits = 134217728 bytes = 128 MiB
(If 4 TB means 4x10^12 bytes: 976562500 blocks, bit vector = 122070312 bytes = about 116 MiB)
FAT for the same disk = 1073741824 entries x 4 bytes = 4294967296 bytes = 4 GiB
Linked free list: no table; 4 bytes in each free block; allocating n blocks needs n reads
Grouping: one block holds 1023 free addresses + 1 link, so an empty disk needs 1048576 group blocks (kept inside free space, no extra memory)
Counting (start,count) 8 bytes per run: best case a few entries; worst case alternating used/free = 536870912 runs x 8 = 4294967296 bytes = 4 GiB
~~~

### output_fat_vs_index.txt
~~~
File: 1048576 blocks (4 GiB). Times are in-memory look-ups only, warm cache. 5 runs each.
method    k          lookups      ns per run (5 runs, sorted)    median ns 
FAT       100        100             205.2    206.5    207.5    210.5    213.9      207.5
index     100        2                 2.0      2.0      2.0      2.1      2.1        2.0
FAT       10000      10000        131356.6 133534.4 139682.8 142360.1 193720.2   139682.8
index     10000      3                 1.6      1.7      1.7      1.9      1.9        1.7
FAT       1048575    1048575      24899413.0 29983507.0 30060544.0 39683953.0 40585961.0 30060544.0
index     1048575    3                 1.4      1.5      1.7      2.1      2.1        1.7
lookups column: FAT = memory look-ups (+1 disk read); index = disk reads incl. the data block.
consistency: FAT and index found the same data block for every k.
~~~

## Program outputs (copied from the runs)

### output_alloc.txt
~~~
AAAAAA........CCCCC.............
E (10 blocks) contiguous: start 19
E (10 blocks) indexed: index block 6, data blocks 7 8 9 10 11 12 13 29 30 31
AAAAAAeEEEEEEECCCCCEEEEEEEEEEEEE
~~~

### output_boundary.txt
~~~
Start: AAAAAA........CCCCC.............
free blocks = 21, largest free run = 13

contiguous E size 13 ->  19   AAAAAA........CCCCCEEEEEEEEEEEEE
contiguous E size 14 ->  -1   AAAAAA........CCCCC.............
indexed    E size 20 ->   6   AAAAAAeEEEEEEECCCCCEEEEEEEEEEEEE
indexed    E size 21 ->  -1   AAAAAA........CCCCC.............
~~~

### output_calc.txt
~~~
=== 2.1 Workload numbers (assumptions [A]: 32 cameras, 4 Mbit/s each, 1-hour segment files) ===
Bytes per second per camera   = 4 x 1,000,000 / 8 = 500000
Bytes per second, all cameras = 500000 x 32 = 16000000
Block appends per second      = 16000000 / 4096 = 3906 (all cameras)
Segment size (1 hour)         = 500000 x 3600 = 1800000000 bytes = 439454 blocks
New segment files per day     = 32 x 24 = 768 (also 768 deletes per day once 30 days are full)
Storage for 30 days           = 16000000 x 86400 x 30 = 41472000000000 bytes (about 41.5 TB)
A 4 TB (4x10^12) disk lasts at 16000000 B/s: 250000 s (about 2.9 days)

=== 2.3 Cost to reach the last block of a 4 GB file (assumption [A]: 4 GB = 4 GiB, directory entry / inode already in memory) ===
Blocks in file = 4294967296 / 4096 = 1048576 ; last block number (from 0) = 1048575
Contiguous : address = start + 1048575, so 1 disk read
Linked     : must follow the chain from block 0, so 1048576 disk reads
Linked, with the 4-byte pointer stored inside each block: 4092 usable bytes, so 1049602 blocks and 1049602 disk reads
FAT        : 1048575 memory look-ups in the FAT + 1 disk read for the data block
Indexed    : pointers per block = 4096 / 4 = 1024
  direct covers blocks 0..11; single covers 12..1035; double covers 1036..1049611
  block 1048575 is in the double-indirect range: double-indirect block + single-indirect block + data block = 3 disk reads

=== 2.3 Maximum file size of the inode (12 direct, 1 single, 1 double, 1 triple, 4-byte addresses) ===
Direct : 12 x 4096                = 49152 bytes (48 KiB)
Single : 1024 x 4096              = 4194304 bytes (4 MiB)
Double : 1024 x 1024 x 4096       = 4294967296 bytes (4 GiB)
Triple : 1024 x 1024 x 1024 x 4096 = 4398046511104 bytes (4 TiB)
Total  : 4402345721856 bytes (4 TiB + 4 GiB + 4 MiB + 48 KiB)
Blocks : 12 + 1024 + 1048576 + 1073741824 = 1074791436
A 32-bit block number can address 2^32 blocks x 4096 = 17592186044416 bytes (16 TiB), so addressing is not the limit.

=== 2.4 Free-space management for a 4 TB disk (assumption [A]: 4 TB = 4 TiB = 2^42 bytes, 4 KiB blocks) ===
Blocks = 2^42 / 4096 = 1073741824
Bit vector = 1073741824 bits = 134217728 bytes = 128 MiB
(If 4 TB means 4x10^12 bytes: 976562500 blocks, bit vector = 122070312 bytes = about 116 MiB)
FAT for the same disk = 1073741824 entries x 4 bytes = 4294967296 bytes = 4 GiB
Linked free list: no table; 4 bytes in each free block; allocating n blocks needs n reads
Grouping: one block holds 1023 free addresses + 1 link, so an empty disk needs 1048576 group blocks (kept inside free space, no extra memory)
Counting (start,count) 8 bytes per run: best case a few entries; worst case alternating used/free = 536870912 runs x 8 = 4294967296 bytes = 4 GiB
~~~

### output_fat_vs_index.txt
~~~
File: 1048576 blocks (4 GiB). Times are in-memory look-ups only, warm cache. 5 runs each.
method    k          lookups      ns per run (5 runs, sorted)    median ns 
FAT       100        100             192.2    200.0    211.4    212.4    231.4      211.4
index     100        2                 1.8      1.9      1.9      1.9      1.9        1.9
FAT       10000      10000        124095.4 124289.8 127576.9 128862.8 133411.2   127576.9
index     10000      3                 1.6      1.7      1.7      1.9      2.0        1.7
FAT       1048575    1048575      15181343.0 16270547.0 16802045.0 18065604.0 22223775.0 16802045.0
index     1048575    3                 1.5      1.6      1.8      1.8      2.0        1.8
lookups column: FAT = memory look-ups (+1 disk read); index = disk reads incl. the data block.
consistency: FAT and index found the same data block for every k.
~~~
