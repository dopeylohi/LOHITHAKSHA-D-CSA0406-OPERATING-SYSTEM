# Task 5 – Distributed and Cloud-Based Storage, Device Management (7 marks)

A university wants all student and staff files available from any campus and from home.

Labels: [C] calculated (`output_calc5.txt`), [T] typical published value (not measured by me; check current vendor documentation and pricing), [A] assumed, [M] measured on my machine (`output_dev.txt`).

## 5.1 Comparison of the options (3 marks)

| Criterion | NFS (one file server) | Distributed file system (HDFS, Ceph) | Cloud object storage (S3, Google Cloud Storage) |
|-----------|-----------------------|--------------------------------------|--------------------------------------------------|
| Consistency | Close-to-open: a client sees another client's writes after the writer closes and the reader opens the file; clients cache data and attributes, so reads can be briefly stale | Strong: HDFS has one writer per file and readers see data after close or flush; Ceph (RADOS) acknowledges a write only after all replicas have it | Strong read-after-write for objects and listings in S3 (since 2020) and Google Cloud Storage [T]; no partial in-place updates, an object is replaced as a whole |
| Availability | One server is a single point of failure unless a standby pair is added; maintenance means downtime | Survives the loss of disks, nodes or a rack because of replicas (3 copies in HDFS); metadata service is made redundant (HDFS HA NameNode, Ceph monitors) | Provider designs for about 99.99% availability and 99.999999999% durability for standard classes [T]; depends on the internet link to reach it |
| Performance | Low latency on the campus LAN, POSIX behaviour, good for many small files; limited by the one server's network link and disks | Very high total throughput that grows with the number of nodes; best for large sequential files; HDFS is poor for many small files; needs a client or gateway | Tens of milliseconds to the first byte, very high total throughput; request-based, not a normal directory tree; slow for small random updates |
| Cost | Cheapest to start: one server and its disks; staff time to run and back up | Hardware for 3 copies of all data (3x raw disks) plus skilled staff | No hardware: about $0.023 per GB-month for standard storage and about $0.09 per GB for data sent to the internet [T, approximate]; 100 TB stored = 100,000 GB x 0.023 = about $2,300 per month [C]; cost grows with transfer |

### Recommendation
Choose a replicated distributed file system (Ceph, with a file and an S3-compatible gateway) running on the campuses for the main student and staff files: replicas across two campuses give availability when a server or a site fails, strong consistency suits documents and coursework, and throughput grows by adding nodes. Use cloud object storage for off-site backup and cold archives, where a one-copy price and high durability matter more than speed. Keep plain NFS only for a single lab or a small department, where one server is enough and a short outage is acceptable. Home access goes through a web or sync gateway in front of the cluster.

## 5.2 Replication and consistency (2 marks)

### How replication protects data
- Each block is stored on several machines; HDFS keeps 3 copies by default, usually two in one rack and one in another [T].
- If a disk or node fails, the system notices (missing heartbeats), copies the lost blocks from the surviving replicas to other nodes, and restores 3 copies, so no data is lost and reads are served from a replica meanwhile.
- Reads can use the nearest or least busy copy, which also improves read throughput.
- Chance of losing a block [C], assuming copies fail independently with probability p = 0.01 before they are re-replicated [A]: 1 copy loses it with probability 0.01; 3 copies only if all three fail, p x p x p = 0.000001, which is 10,000 times smaller. Independence is an assumption: copies in the same rack or on the same power share failures, which is why replicas are placed in different racks.
- Cost: 3 times the raw storage (200% extra). Erasure coding (for example 6 data + 3 parity blocks, 1.5x raw) is a cheaper alternative for cold data [T].

### Strong against eventual consistency
- **Strong consistency:** after a write completes, every reader sees it. The write is acknowledged only after enough replicas have it, so writes are slower and, during a network partition, some operations must wait or fail (the CAP trade-off). Use it for marks, accounts and documents being edited.
- **Eventual consistency:** a write is acknowledged after one replica has it and copied to the others later. Writes are fast and the system keeps working during a partition, but a reader may see old data for a while and concurrent writes can conflict. Use it for view counts, caches and search indexes.
- **Quorum rule [C]:** with N replicas, a write acknowledged by W and a read from R replicas is strong when R + W > N, because the read set and the write set must overlap. For N = 3: W = 2 and R = 2 is strong; W = 1 and R = 1 is fast but can read stale data. See the table in `output_calc5.txt`.

## 5.3 Device management (2 marks)

Path of a read request: application -> system call -> file system -> page cache (hit: done) -> block layer with its request queue and I/O scheduler -> device driver -> controller and DMA -> device -> interrupt -> data copied back.

- **Device driver:** kernel code that knows one kind of hardware. It turns generic requests ("read block 500") into device commands (controller registers, DMA setup), handles the completion interrupt and reports errors. It hides hardware differences from the rest of the OS.
- **Buffering:** a memory area that holds data in transit between two parties of different speeds or sizes (a program writing small pieces, a disk writing whole blocks). It also lets a program continue after a write is accepted (write-behind) and supports double buffering.
- **Caching:** a copy of data that also exists on the slower device, kept in fast memory for reuse. The page cache keeps recently used blocks and does read-ahead; a repeated read costs a memory access instead of a disk access. A buffer holds data in transit once; a cache keeps it for reuse.
- **Spooling:** output for a device that serves one job at a time (a printer) is first written to a queue on disk and a daemon sends the jobs to the device one by one. Many programs can print at once without interleaving their output.
- **Block-device interface:** the device appears as an array of fixed-size blocks that can be read or written at any block number (random access), through a file such as `/dev/sdd`. File systems and the cache are built on this. Character devices (keyboard, terminal) are streams of bytes without random access.

Evidence from my machine [M]: `output_dev.txt` shows `b` (block) for the disks and `c` (character) for `/dev/null` and `/dev/tty`, the block-device drivers registered in the kernel, and how much memory is used for buffers, the page cache and dirty data waiting to be written back.

## Alternative: one large NFS server instead of a replicated distributed file system

**Solution.** A single NFS server with RAID disks, a fast network link and a backup, exporting home directories to all campuses and to home users through a VPN. A standby server and regular backups reduce the damage of a failure.

Assumptions [A]: 100 MB file; client link 1 Gbit/s (125 MB/s); NFS server link 10 Gbit/s (1,250 MB/s); cluster of 10 nodes with 10 Gbit/s each and 3 replicas; disks are not the limit; all users active together. Per-user rate = the smaller of the client link and (total bandwidth / users). Results from `output_calc5.txt`:

| Users | NFS read and write (s) | Cluster read (s) | Cluster write (s) |
|-------|------------------------|------------------|-------------------|
| 1 | 0.8 | 0.8 | 0.8 |
| 10 | 0.8 | 0.8 | 0.8 |
| 100 | 8.0 | 0.8 | 2.4 |
| 1000 | 80.0 | 8.0 | 24.0 |

- **Read and write time:** the single NFS server is shared by everyone, so time grows in proportion to the number of users once the server link saturates (at 1000 users each gets 1.25 MB/s). The cluster's total bandwidth grows with the number of nodes. Writes in the cluster are 3 times slower than reads because each block is written to 3 replicas.
- **Storage used:** 100 TB of data needs 100 TB for one copy (plus RAID overhead) and 300 TB with 3 replicas [C]; NFS is 3 times cheaper in disks.
- **Availability when one server fails:** with each server up 99% of the time [A], one NFS server is available 99% of the time (about 87.6 hours of downtime a year [C]), and everyone loses access when it fails. With 3 independent replicas, data is unavailable only if all three are down: 0.999999 availability, about 32 seconds a year [C]. This assumes independent failures and a redundant metadata service.
- **Verdict:** one NFS server is simple and cheap and fine for a small group, but it is a bottleneck and a single point of failure for a whole university.

## Program outputs (copied from the runs)

### output_calc5.txt
~~~
=== 5.2 Chance of losing all copies (assumption [A]: copies fail independently, p = chance a copy is lost before it is re-replicated) ===
p = 0.0100
1 copy : 0.010000
2 copies: 0.000100
3 copies: 0.000001 (= p x p x p)

=== 5.2 Quorum rule: strong consistency needs R + W > N ===
N=3 W=1 R=1  R+W=2  -> may read stale data
N=3 W=1 R=2  R+W=3  -> may read stale data
N=3 W=1 R=3  R+W=4  -> strong
N=3 W=2 R=1  R+W=3  -> may read stale data
N=3 W=2 R=2  R+W=4  -> strong
N=3 W=2 R=3  R+W=5  -> strong
N=3 W=3 R=1  R+W=4  -> strong
N=3 W=3 R=2  R+W=5  -> strong
N=3 W=3 R=3  R+W=6  -> strong

=== Alternative: one NFS server against a replicated cluster ===
Assumptions [A]: file 100 MB; client link 1 Gbit/s = 125 MB/s; NFS server link 10 Gbit/s = 1250 MB/s;
cluster of 10 nodes, each with a 10 Gbit/s link; 3 replicas; disks are not the limit; all users busy at the same time.
users    NFS rd/wr (s) cluster rd (s) cluster wr (s)
1                  0.8            0.8            0.8
10                 0.8            0.8            0.8
100                8.0            0.8            2.4
1000              80.0            8.0           24.0

NFS: per-user rate with 1000 users = 1250 / 1000 = 1.25 MB/s
Cluster read: 10 x 1250 = 12500 MB/s in total; write: 12500 / 3 = 4167 MB/s in total

Storage for 100 TB of user data: NFS one copy = 100 TB; 3 replicas = 300 TB (2 TB of extra copies for every 1 TB)

Availability (assumption [A]: each server is up 99% of the time, failures independent)
One NFS server : 0.9900  (down 87.6 hours per year)
3 replicas     : 0.999999  (all three down 32 seconds per year)
~~~

### output_dev.txt
~~~
=== Device files: first letter b = block device, c = character device ===
crw-rw-rw- 1 root root 1,  3 Nov 22 03:20 /dev/null
brw-rw---- 1 root disk 8,  0 Nov 22 03:20 /dev/sda
brw-rw---- 1 root disk 8, 48 Nov 22 03:20 /dev/sdd
crw-rw-rw- 1 root tty  5,  0 Nov 22 12:10 /dev/tty

=== Block device drivers registered in the kernel ===
Block devices:
  1 ramdisk
  7 loop
  8 sd
 11 sr
 65 sd
 66 sd
 67 sd
 68 sd
 69 sd
 70 sd
 71 sd
128 sd

=== Memory used as buffers, page cache and dirty (not yet written) data ===
MemTotal:        9154212 kB
Buffers:            7016 kB
Cached:           147264 kB
Dirty:                56 kB
Writeback:             0 kB
~~~
