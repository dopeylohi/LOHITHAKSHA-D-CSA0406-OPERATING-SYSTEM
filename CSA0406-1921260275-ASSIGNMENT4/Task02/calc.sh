#!/bin/bash
# Task 2 calculations. [A] = assumption, [C] = calculated with the working shown.
BLK=4096                      # [A] 4 KiB block (given)
PTR=4                         # 4-byte block address (given)
P=$((BLK / PTR))              # pointers per block

echo "=== 2.1 Workload numbers (assumptions [A]: 32 cameras, 4 Mbit/s each, 1-hour segment files) ==="
CAMS=32; MBPS=4; SEGH=1
BPS=$((MBPS * 1000000 / 8))
AGG=$((BPS * CAMS))
SEGB=$((BPS * 3600 * SEGH))
SEGBLK=$(( (SEGB + BLK - 1) / BLK ))
echo "Bytes per second per camera   = $MBPS x 1,000,000 / 8 = $BPS"
echo "Bytes per second, all cameras = $BPS x $CAMS = $AGG"
echo "Block appends per second      = $AGG / $BLK = $((AGG / BLK)) (all cameras)"
echo "Segment size (1 hour)         = $BPS x 3600 = $SEGB bytes = $SEGBLK blocks"
echo "New segment files per day     = $CAMS x 24 = $((CAMS * 24)) (also $((CAMS * 24)) deletes per day once 30 days are full)"
TOT=$((AGG * 86400 * 30))
T10=$(( (TOT + 50000000000) / 100000000000 ))
echo "Storage for 30 days           = $AGG x 86400 x 30 = $TOT bytes (about $((T10 / 10)).$((T10 % 10)) TB)"
SEC=$((4000000000000 / AGG))
D10=$(( (SEC * 10 + 43200) / 86400 ))
echo "A 4 TB (4x10^12) disk lasts at $AGG B/s: $SEC s (about $((D10 / 10)).$((D10 % 10)) days)"

echo
echo "=== 2.3 Cost to reach the last block of a 4 GB file (assumption [A]: 4 GB = 4 GiB, directory entry / inode already in memory) ==="
FILE=$((4 * 1024 * 1024 * 1024))
NB=$((FILE / BLK))
LAST=$((NB - 1))
echo "Blocks in file = $FILE / $BLK = $NB ; last block number (from 0) = $LAST"
echo "Contiguous : address = start + $LAST, so 1 disk read"
echo "Linked     : must follow the chain from block 0, so $NB disk reads"
USABLE=$((BLK - PTR))
NBL=$(( (FILE + USABLE - 1) / USABLE ))
echo "Linked, with the 4-byte pointer stored inside each block: $USABLE usable bytes, so $NBL blocks and $NBL disk reads"
echo "FAT        : $LAST memory look-ups in the FAT + 1 disk read for the data block"
S_START=12
D_START=$((12 + P))
T_START=$((12 + P + P * P))
echo "Indexed    : pointers per block = $BLK / $PTR = $P"
echo "  direct covers blocks 0..$((S_START - 1)); single covers $S_START..$((D_START - 1)); double covers $D_START..$((T_START - 1))"
if   [ "$LAST" -lt "$S_START" ]; then L=1
elif [ "$LAST" -lt "$D_START" ]; then L=2
elif [ "$LAST" -lt "$T_START" ]; then L=3
else L=4; fi
echo "  block $LAST is in the double-indirect range: double-indirect block + single-indirect block + data block = $L disk reads"

echo
echo "=== 2.3 Maximum file size of the inode (12 direct, 1 single, 1 double, 1 triple, 4-byte addresses) ==="
DIR=$((12 * BLK))
SIN=$((P * BLK))
DOU=$((P * P * BLK))
TRI=$((P * P * P * BLK))
echo "Direct : 12 x $BLK                = $DIR bytes (48 KiB)"
echo "Single : $P x $BLK              = $SIN bytes (4 MiB)"
echo "Double : $P x $P x $BLK       = $DOU bytes (4 GiB)"
echo "Triple : $P x $P x $P x $BLK = $TRI bytes (4 TiB)"
echo "Total  : $((DIR + SIN + DOU + TRI)) bytes (4 TiB + 4 GiB + 4 MiB + 48 KiB)"
echo "Blocks : 12 + $P + $((P * P)) + $((P * P * P)) = $((12 + P + P * P + P * P * P))"
echo "A 32-bit block number can address 2^32 blocks x $BLK = $(( (1 << 32) * BLK )) bytes (16 TiB), so addressing is not the limit."

echo
echo "=== 2.4 Free-space management for a 4 TB disk (assumption [A]: 4 TB = 4 TiB = 2^42 bytes, 4 KiB blocks) ==="
NBD=$(( (1 << 42) / BLK ))
echo "Blocks = 2^42 / $BLK = $NBD"
echo "Bit vector = $NBD bits = $((NBD / 8)) bytes = $((NBD / 8 / 1048576)) MiB"
NBD10=$((4000000000000 / BLK))
echo "(If 4 TB means 4x10^12 bytes: $NBD10 blocks, bit vector = $((NBD10 / 8)) bytes = about $((NBD10 / 8 / 1048576)) MiB)"
echo "FAT for the same disk = $NBD entries x 4 bytes = $((NBD * 4)) bytes = $((NBD * 4 / 1073741824)) GiB"
echo "Linked free list: no table; 4 bytes in each free block; allocating n blocks needs n reads"
echo "Grouping: one block holds $((P - 1)) free addresses + 1 link, so an empty disk needs $((NBD / P)) group blocks (kept inside free space, no extra memory)"
echo "Counting (start,count) 8 bytes per run: best case a few entries; worst case alternating used/free = $((NBD / 2)) runs x 8 = $((NBD / 2 * 8)) bytes = $((NBD / 2 * 8 / 1073741824)) GiB"
