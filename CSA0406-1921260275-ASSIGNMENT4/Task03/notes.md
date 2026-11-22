# Task 3 – Modern File Systems (9 marks)

Choosing a file system for different devices.

Labels: [M] measured (from `output.txt`), [C] calculated, [T] typical published value (kernel documentation, vendor documentation, Wikipedia "Comparison of file systems"; these were not measured by me), [A] assumed.

## 3.1 Comparison of modern file systems (3 marks)

| File system | Max file size [T] | Max volume size [T] | Journaling | Snapshots | Checksums | Copy-on-write |
|-------------|-------------------|---------------------|------------|-----------|-----------|---------------|
| FAT32 | 4 GiB - 1 byte | 2 TiB with 512-byte sectors | No | No | No | No |
| NTFS | 16 EiB in the format; 256 TiB in current Windows (64 KiB clusters) | Same limits as file size | Yes (metadata, `$LogFile`) | Through Volume Shadow Copy (VSS), not in the file system itself | No for data | No |
| ext4 | 16 TiB (4 KiB blocks) | 1 EiB (64-bit feature), 16 TiB without it | Yes (jbd2: journal, ordered, writeback) | No (use LVM snapshots) | Metadata and journal checksums (metadata_csum); no data checksums | No |
| XFS | 8 EiB | 8 EiB | Yes (metadata) | No native snapshots (use LVM or xfsdump) | Metadata CRCs (v5 format); no data checksums | Only reflink file copies; not a CoW file system |
| Btrfs | 16 EiB | 16 EiB | No journal (CoW plus a log tree for fsync) | Yes, writable subvolume snapshots | Yes, data and metadata (crc32c default) | Yes |
| ZFS | 16 EiB | 256 ZiB per pool | No journal (CoW transactions plus ZIL for synchronous writes) | Yes | Yes, data and metadata (fletcher4 default, SHA-256 option) | Yes |
| APFS | About 8 EiB (64-bit sizes) | About 8 EiB | No journal (copy-on-write metadata) | Yes | Metadata only; no user-data checksums | Yes (metadata, clones, snapshots) |

Reading the table: FAT32 is the simplest and most portable but has a 4 GiB file limit and no crash protection. NTFS and ext4 and XFS keep a journal for metadata consistency. Btrfs, ZFS and APFS never overwrite live blocks (copy-on-write), which gives cheap snapshots; Btrfs and ZFS add end-to-end checksums so silent corruption is detected.

## 3.2 Journaling and consistency (2 marks)

### What happens when power fails during a write
Appending a block to a file needs several separate updates: the new data block, the inode (size and block pointer), and the allocation bitmap (and the directory if the file is new). The disk can complete only some of them. Examples: the bitmap says a block is used but no inode points to it (a leaked block); an inode points to a block that the bitmap still shows as free (the block can be given to another file, so two files share one block); the inode size is updated but the data was never written (file contains garbage or zeros). A file system without protection needs a full check (`fsck`) of the whole disk after a crash, which takes longer the bigger the disk.

### Journaling (write-ahead log)
1. The file system writes the planned updates as one transaction to the journal.
2. It writes a commit record. The transaction now counts as done.
3. Later it copies (checkpoints) the changes to their real places.
After a crash, recovery replays committed transactions and discards uncommitted ones, so the metadata is always consistent. Recovery time depends on the journal size, not the disk size.

### Copy-on-write
Live blocks are never overwritten. The new data and new copies of every changed metadata block (up to the root of the tree) are written to free space, and then one root pointer (superblock or uberblock) is switched in a single atomic write. After a crash the file system sees either the complete old tree or the complete new tree. Nothing needs replaying, and the old tree is what a snapshot keeps.

### ext4 journaling modes
| Mode | What is journaled | Ordering | State after a crash | Speed |
|------|-------------------|----------|---------------------|-------|
| `data=journal` | Data and metadata | Everything goes through the journal first | Safest: data and metadata agree | Slowest: data is written twice |
| `data=ordered` (default) | Metadata only | Data blocks are forced to disk before the metadata commit | Metadata consistent; a file never points to stale data, but the last write may be lost | Medium |
| `data=writeback` | Metadata only | No ordering between data and metadata | Metadata consistent, but a file can contain old or garbage data | Fastest |

## 3.3 Choice for each device (3 marks)

| Device | Choice | Justification |
|--------|--------|---------------|
| Pen drive used on Windows and Linux | exFAT (FAT32 only if the drive must also work on very old devices and every file is under 4 GiB) | Both Windows and Linux can read and write exFAT, files can be larger than 4 GiB, and it is light on flash. ext4 is not readable on Windows without extra tools; NTFS works on Linux but permissions do not map. No journal, so eject safely before unplugging |
| Linux database server | ext4 with `data=ordered` (XFS is an equally valid choice for large parallel I/O) | Mature, predictable update-in-place writes, journal gives fast recovery, small write amplification for random updates, good fsync behaviour |
| Laptop | ext4 on a Linux laptop (NTFS on a Windows laptop, APFS on a Mac) | Each is the native file system of its OS, journaled or crash-safe, good SSD support (TRIM). Btrfs is an option on Linux if the user wants snapshots before updates |
| Backup server that needs snapshots | ZFS (Btrfs where ZFS is not suitable) | Cheap copy-on-write snapshots, checksums plus scrubbing detect silent corruption, incremental snapshot send/receive for replication, compression |

## 3.4 Inspect a real file system (1 mark)

Run on my Ubuntu 26.04.1 under WSL2 with `task03.sh`; the full output is in `output.txt` and the full `tune2fs -l` listing in `tune2fs_full.txt`. Output is at the end of this file.

What each command shows:
- `df -hT` and `findmnt`: device, file-system type, size, used and free space, mount options (the `data=` option shows the journaling mode in use if the kernel lists it).
- `df -i`: total, used and free inodes.
- `ls -i`: inode number of each name. The file and its hard link show the same inode number, so the name is only a directory entry and the inode holds the file.
- `stat`: size, blocks, I/O block size, inode number, link count, times. A 5000-byte file occupies 16 sectors of 512 bytes, which is 2 blocks of 4096 bytes: the last block is only partly used (internal fragmentation).
- `stat -f`: block size and the block and inode totals of the whole file system.
- `tune2fs -l`: block size, block count, free blocks, reserved blocks, inode count, free inodes, inode size, features (`has_journal` shows the journal) and the journal inode. The script calculates free space as free blocks x block size [C] and checks it against df.
- The counters in a mounted file system's superblock can lag slightly behind what `df` shows, so small differences are normal.

## Alternative: copy-on-write file system (Btrfs or ZFS) for the database server

**Solution.** Put the database on Btrfs or ZFS instead of ext4 with journaling. Use a small record size for the database page size (ZFS `recordsize` of 8 to 16 KiB; Btrfs: keep checksums on, or mark the database directory no-CoW with `chattr +C`, which also turns checksums off). Take snapshots for backups. The database's own write-ahead log still protects transactions.

| Measure | ext4 with journal | Btrfs / ZFS (copy-on-write) |
|---------|-------------------|------------------------------|
| Write time for a small random update | The data block is rewritten in place; only metadata goes through the journal. About 1 data block plus a share of one journal transaction [A, not measured] | The new data block and every metadata block up the tree are rewritten: 1 + h blocks, where h is the tree height (often 3 to 4 for a large volume [A]), so O(log N) blocks per update instead of O(1). Random updates also scatter the file over time (fragmentation) |
| Extra space for snapshots | None (no native snapshots) | Only blocks changed since the snapshot. If 1 GiB of the database is rewritten while a snapshot exists, up to 1 GiB of old blocks is kept [C, example]. A write-heavy database makes snapshots grow quickly |
| Extra space for checksums | Metadata checksums only, tiny | Btrfs crc32c = 4 bytes per 4096-byte block = 0.098% [C]; SHA-256 = 32 / 4096 = 0.78% [C] |
| Recovery after a power failure | Journal replay; time grows with journal size, normally seconds [T]; no full `fsck` needed | No replay of the file system: it mounts at the last committed root. Btrfs commits about every 30 seconds [T]; synchronous writes are recovered from the log tree (Btrfs) or ZIL (ZFS) |

No copy-on-write file system was available on my machine, so these comparisons are reasoning and typical values, not measurements. Verdict: for small random updates ext4 (or XFS) is cheaper per write; Btrfs/ZFS win on snapshots, corruption detection and instant mount after a crash.

## Program outputs (copied from the run)

### output.txt
~~~
=== Where this folder lives ===
Device: /dev/sdd
ext4 rw,relatime,discard,errors=remount-ro,data=ordered
Filesystem     Type  Size  Used Avail Use% Mounted on
/dev/sdd       ext4 1007G   19G  938G   2% /

=== df -i (inodes) ===
Filesystem       Inodes  IUsed    IFree IUse% Mounted on
/dev/sdd       67108864 100348 67008516    1% /

=== Test files: one 5000-byte file and a hard link to it ===
19316 -rw-r--r-- 2 lohi lohi 5000 Nov 22 09:07 file5000.bin
19316 -rw-r--r-- 2 lohi lohi 5000 Nov 22 09:07 hardlink.bin

  File: file5000.bin
  Size: 5000      	Blocks: 16         IO Block: 4096   regular file
Device: 8,48	Inode: 19316       Links: 2
Access: (0644/-rw-r--r--)  Uid: ( 1000/    lohi)   Gid: ( 1000/    lohi)
Access: 2026-11-22 09:07:50.685509625 +0000
Modify: 2026-11-22 09:07:50.689509625 +0000
Change: 2026-11-22 09:07:50.693509624 +0000
 Birth: 2026-11-22 09:07:50.685509625 +0000

  File: "."
    ID: 3913e53fdd0831a6 Namelen: 255     Type: ext2/ext3
Block Size: 4096       Fundamental block size: 4096
Blocks: Total: 263940717  Free: 259107849  Available: 245681981
Inodes: Total: 67108864   Free: 67008515

=== tune2fs -l (key lines; full listing saved in tune2fs_full.txt) ===
tune2fs exit=0
Filesystem features:      has_journal ext_attr resize_inode dir_index orphan_file filetype needs_recovery extent 64bit flex_bg metadata_csum_seed sparse_super large_file huge_file dir_nlink extra_isize metadata_csum orphan_present
Default mount options:    user_xattr acl
Filesystem state:         clean
Inode count:              67108864
Block count:              268435456
Reserved block count:     13421772
Free blocks:              259107928
Free inodes:              67008570
Block size:               4096
Inode size:               256
Journal inode:            8
Journal backup:           inode blocks

=== Calculations [C] from the tune2fs numbers ===
Volume size  = 268435456 blocks x 4096 = 1099511627776 bytes = 1024.00 GiB
Free space   = 259107928 blocks x 4096 = 1061306073088 bytes = 988.42 GiB
Reserved     = 13421772 blocks x 4096 = 54975578112 bytes = 51.20 GiB
Free - reserved = 1006330494976 bytes (should be close to the Avail column of df)
df reports Avail (bytes):
1006313394176
Inodes used  = 67108864 - 67008570 = 100294 (compare with IUsed in df -i above)
Bytes of volume per inode = 16384
~~~
