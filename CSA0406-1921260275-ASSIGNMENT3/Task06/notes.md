# Task06 - Disk Management (4 marks)

Setup: Ubuntu 26.04.1 on WSL2. Labels: [M] measured/run, [C] calculated (working shown), [T] textbook or published value, [A] assumed.
Hospital records server [A]: 6 disks of 4 TB, must start reliably, survive a disk failure and have enough swap for peak load.

## 1. Low-level formatting, partitioning and logical formatting [T]
- Low-level (physical) formatting: done at the factory; it divides the surface into sectors, each with a header, a data area and an error-correcting code (ECC), and sets up the spare sectors. The user does not repeat it. (SSDs have no cylinders or sectors in this sense; the controller maps logical blocks to flash.)
- Partitioning: the OS divides the disk into one or more regions (partitions), each treated as a separate device, and records them in a partition table (MBR or GPT).
- Logical formatting: creates the file system inside a partition (mkfs): the root directory, free-space map, inode or allocation tables. Swap space is not logically formatted with a file system; it is prepared with mkswap and used as raw blocks.

## 2. Boot block and boot process [T]
1. Power on: the firmware (BIOS or UEFI) runs its self-test (POST).
2. The firmware reads the boot block: on a BIOS disk the first 512-byte sector (MBR: 446 bytes of boot code, a 64-byte partition table, a 2-byte signature 0x55AA); on UEFI the boot loader file in the EFI system partition.
3. The boot loader (for example GRUB) knows where the kernel is and loads the kernel and an initial RAM disk.
4. The kernel starts, mounts the root file system and starts the first process (systemd), which starts the services.
For the hospital server the boot partition and the boot loader should be on a mirrored (RAID 1) pair so that the failure of one disk does not stop the start-up [A]. In my WSL machine there is no boot block to show: WSL starts the Microsoft-supplied Linux kernel inside a virtual machine, with no GRUB.

## 3. Bad blocks [T]
- Factory: bad sectors are found during low-level formatting and listed.
- Controller: sector sparing (a bad sector is remapped to a spare sector, usually in the same cylinder) or sector slipping (the sectors after the bad one are shifted).
- OS: file systems can mark bad blocks so they are never allocated (badblocks and e2fsck on ext4); in simple file systems such as FAT the marking is done during logical formatting or by scanning.
- SSDs: the controller retires worn-out blocks and uses over-provisioned spare flash.
- RAID: the data of a failing block is rebuilt from the other disks (a periodic "scrub" reads everything to find bad blocks before a rebuild needs them).

## 4. Partitions and swap on Linux (lsblk, fdisk -l) - Figure 6.1 and 6.2
lsblk lists block devices as a tree: name, major:minor, removable, size, read-only, type and mount points. `fdisk -l` prints, for every disk, its size, sector sizes and (if a partition table exists) one line per partition. Output of my machine [M] (disk_out.txt):
| Device | Size | Type / use |
|---|---|---|
| sda | 722.9 MB | read-only virtual disk (WSL system) |
| sdb | 97.4 MB | read-only, mounted at /usr/src/linux-headers-6.18.58.1-microsoft-standard-WSL2 |
| sdc | 4 GB | swap: `swapon --show` = /dev/sdc, partition, 4G, 2.3G used |
| sdd | 1 TiB (1,099,511,627,776 B) | root file system `/`: ext4, rw,relatime,discard,errors=remount-ro,data=ordered; also /var/lib/docker and /mnt/wslg/distro |
- `fdisk -l` printed /dev/sda to /dev/sdd and sixteen 4 MiB RAM disks (/dev/ram0 to /dev/ram15), but no partition lines at all [M]: WSL attaches each virtual disk as a whole device with the file system directly on it, so there is no partition table to list. I did not create partitions, because that would change my system.
- `free -m` at that moment [M]: RAM 8939 MB total, 4359 used, 4473 free; swap 4096 total, 2367 used, 1728 free. The swap use rose from 204 MB at the start of Task04 to 2367 MB, after the memory experiments [M].
- The 1 TiB is the size of the virtual disk, not necessarily data stored (a virtual disk file on the host) [A].

## 5. Swap: size and location, separate disk versus swap file
Size [A]: the server has 64 GB of RAM (assumed). No hibernation, so swap is only a safety net for peak load and for pushing out cold pages. I choose 16 GB = 25% of RAM. More would only delay the moment a runaway process is killed while the server thrashes (Task04: pages beyond the limit made every access fault). The size is checked by watching swap use under real peak load and adjusted.
Location: on a separate partition on a mirrored (RAID 1) pair of fast disks, not on the RAID 6 data array [A], because (a) a single disk holding swap is a single point of failure: if it dies, processes whose pages were in swap crash; (b) swap I/O does not compete with record I/O on the data disks; (c) RAID 5 and 6 are slow at small writes (Section 8) and swapping is made of small writes.
Separate partition versus swap file [T]:
| | Separate partition (or disk) | Swap file |
|---|---|---|
| Resize | needs repartitioning | create or delete a file, simple |
| Speed | direct blocks; separate disk gives parallel I/O | on ext4 the kernel maps the file's blocks once, so speed is close to a partition; shares the disk with the file system |
| Risk | none | the file must not be fragmented or sparse, not allowed on some copy-on-write file systems |
| Choice | recommended for this server (fixed size, separate disk) | good for a laptop or a cloud machine where the size changes |
My own machine uses a separate 4 GB virtual disk /dev/sdc [M].

## 6. RAID comparison (raid.sh) - Table 6.1
Usable capacity, 6 disks of 4 TB, decimal TB [C]:
| RAID | Formula | Usable | In TiB | Disk failures survived [T] | Speed [T] |
|---|---|---|---|---|---|
| 0 (striping) | n x d = 6 x 4 | 24 TB | 21.83 | 0 | reads and writes up to 6 times one disk; no redundancy |
| 1 (mirroring), 3 pairs | 3 x 4 | 12 TB (one 6-way mirror would give 4 TB) | 10.91 | 1 per pair | reads up to 2 times per pair; writes as one disk |
| 5 (one parity) | (n - 1) x d = 5 x 4 | 20 TB | 18.19 | 1 | reads about 5 times; small write = 4 I/Os |
| 6 (two parities) | (n - 2) x d = 4 x 4 | 16 TB | 14.55 | any 2 | reads about 4 times; small write = 6 I/Os |
| 10 (stripe of mirrors) | n/2 x d = 3 x 4 | 12 TB | 10.91 | 1 guaranteed, up to 3 if each is in a different pair | reads up to 6 times; writes about 3 times one disk |
(1 TB = 10^12 B and 1 TiB = 2^40 B, so 4 TB = 3.64 TiB.)

## 7. Probability of data loss (raid.sh) - Table 6.2
Assumptions [A]: annual failure rate AFR = 2% per disk (published figures are about 1% to 3% [T]); the array is exposed for 24 h after a failure (replace + rebuild); failures are independent. q = chance that one given surviving disk fails inside that window = 0.02 x 24 / 8760 = 5.4795 x 10^-5 [C].
Given that one disk has failed [C]:
| RAID | Data is lost if | Probability |
|---|---|---|
| 1 (2-disk mirror) | the one partner fails | q = 5.479 x 10^-5 |
| 10 (3 mirrors) | the partner of the failed disk fails | q = 5.479 x 10^-5 |
| 5 (6 disks) | any of the other 5 disks fails | 1 - (1 - q)^5 = 2.739 x 10^-4 (about 5q) |
| 6 (6 disks) | 2 or more of the other 5 fail | 1 - (1 - q)^5 - 5q(1 - q)^4 = 3.002 x 10^-8 (about 10 q^2) |
Per year [C]: chance that at least one disk fails: 2 disks 1 - 0.98^2 = 0.0396; 6 disks 1 - 0.98^6 = 0.1142. Multiplied by the values above:
| RAID | Data loss per year | About 1 in |
|---|---|---|
| 1 (2 disks) | 0.0396 x 5.479e-5 = 2.17e-6 | 460,859 |
| 10 (6 disks) | 0.1142 x 5.479e-5 = 6.26e-6 | 159,867 |
| 5 (6 disks) | 0.1142 x 2.739e-4 = 3.13e-5 | 31,977 |
| 6 (6 disks) | 0.1142 x 3.002e-8 = 3.43e-9 | 291,788,676 |
- RAID 5 is 159,867 / 31,977 = 5.0 times more likely to lose data than RAID 10, and 291,788,676 / 31,977 = 9,125 times more likely than RAID 6 [C].
- Unrecoverable read error (URE) during a rebuild [T, C]: a typical consumer disk specification is 1 error in 10^14 bits read. A RAID 5 rebuild reads 5 x 4 TB = 20 TB = 1.6 x 10^14 bits, expected errors 1.6, so P(at least one) = 1 - e^-1.6 = 79.8%; a mirror rebuild reads 4 TB = 3.2 x 10^13 bits, expected 0.32, P = 27.4%. RAID 6 can still repair the read error because it has a second parity. These are datasheet worst cases; real disks are usually better, and enterprise disks (10^-15) are ten times better, so the real risk is lower, but the ranking is the same.
- Model limits: failures are assumed independent, with a constant rate; real disks from one batch fail together more often, so the absolute numbers are optimistic. I use them to compare the levels, not as predictions.

## 8. Write cost [T]
A small write needs: RAID 0: 1 write; RAID 1 and 10: 2 writes; RAID 5: 4 I/Os (read old data, read old parity, write new data, write new parity); RAID 6: 6 I/Os (also read and write the second parity).

## 9. Read speed on SSD and hard disk
Not measured. Only an SSD is available on this PC (stated by the student), so there is no hard disk to compare with. WSL also hides the physical disk behind a virtual disk, so a number from it would not be a clean SSD figure. I do not report one.

## 10. Conclusion - configuration recommended for the records server
- 6 x 4 TB as RAID 6: 16 TB usable, any two disks can fail. It has the lowest loss probability by a very wide margin (1 in 291,788,676 per year against 1 in 31,977 for RAID 5 and 1 in 159,867 for RAID 10 [C]) and it survives the read error during a rebuild that RAID 5 meets 79.8% of the time [T, C]. It gives 4 TB more than RAID 10 (16 against 12) and 4 TB less than RAID 5 (16 against 20) [C].
- Cost of the choice: a small write needs 6 I/Os, three times a mirror's 2 [T]. If the measured load turns out to be mostly small random writes (a transaction database), use RAID 10 instead, which gives 12 TB and loses data in only 1 of 159,867 years in my model; I would not use RAID 5 with 4 TB disks.
- Around it: operating system, boot loader and 16 GB swap on a separate RAID 1 pair; a hot spare so that rebuilds begin at once (assumed); periodic scrubbing; monitoring; a UPS; and a backup copy of the records on other media, because RAID protects against disk failure, not against deletion, ransomware or fire.

## 11. Alternative: RAID 6 instead of RAID 5 (or RAID 10)
| | RAID 5 | RAID 6 | RAID 10 |
|---|---|---|---|
| Usable capacity, 6 x 4 TB [C] | 20 TB | 16 TB (20% less than RAID 5: 4 / 20; 33% more than RAID 10: 4 / 12) | 12 TB |
| Time to write a block [T] | 4 I/Os, one parity (XOR) | 6 I/Os; two parities: P by XOR and Q by Reed-Solomon arithmetic over GF(2^8), more CPU work than XOR | 2 writes, no calculation |
| Disk failures survived [T] | 1 | 2 (any) | 1 guaranteed, up to 3 |
Complexity [T]: for n disks, computing P costs O(n) XORs per stripe and Q costs O(n) multiplications in GF(2^8); the space used is 2 disks of 6 (33%) for RAID 6, 1 disk (17%) for RAID 5. I did not measure the parity time or the write speed, so no timings are claimed.

## 12. Limits
- The RAID figures are calculations with assumed AFR, rebuild window and independence, and disk-datasheet error rates; nothing here was measured on an array.
- Swap size and RAM of the server are assumed (64 GB RAM, 16 GB swap).
- The partition and swap listing is from a WSL virtual machine; there is no partition table to show, and no SSD-versus-HDD speed test was possible.
