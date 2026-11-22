#!/bin/bash
# Task 3.4: inspect the real file system that holds this folder (stat, df -i, ls -i, tune2fs -l)
cd "$(dirname "$0")" || { echo "cd failed"; exit 1; }
if [ "$(id -u)" -eq 0 ]; then SUDO=""; else SUDO="sudo"; fi
DEV="${1:-$(findmnt -no SOURCE -T .)}"
DEV="${DEV%%\[*}"

echo "=== Where this folder lives ==="
echo "Device: $DEV"
findmnt -no FSTYPE,OPTIONS -T . 2>/dev/null
df -hT .

echo
echo "=== df -i (inodes) ==="
df -i .

echo
echo "=== Test files: one 5000-byte file and a hard link to it ==="
head -c 5000 /dev/zero > file5000.bin
ln file5000.bin hardlink.bin
ls -li file5000.bin hardlink.bin
echo
stat file5000.bin
echo
stat -f .
rm -f file5000.bin hardlink.bin

echo
echo "=== tune2fs -l (key lines; full listing saved in tune2fs_full.txt) ==="
sync
$SUDO tune2fs -l "$DEV" > tune2fs_full.txt 2>&1
echo "tune2fs exit=$?"
grep -E 'features|Block size|Inode size|Inode count|Free inodes|Block count|Free blocks|Reserved block count|mount options|Journal|Filesystem state' tune2fs_full.txt

echo
echo "=== Calculations [C] from the tune2fs numbers ==="
BS=$(awk -F: '/^Block size/ {gsub(/ /,"",$2); print $2}' tune2fs_full.txt)
BC=$(awk -F: '/^Block count/ {gsub(/ /,"",$2); print $2}' tune2fs_full.txt)
FB=$(awk -F: '/^Free blocks/ {gsub(/ /,"",$2); print $2}' tune2fs_full.txt)
RB=$(awk -F: '/^Reserved block count/ {gsub(/ /,"",$2); print $2}' tune2fs_full.txt)
IC=$(awk -F: '/^Inode count/ {gsub(/ /,"",$2); print $2}' tune2fs_full.txt)
FI=$(awk -F: '/^Free inodes/ {gsub(/ /,"",$2); print $2}' tune2fs_full.txt)
if [ -z "$BS" ] || [ -z "$FB" ] || [ -z "$IC" ]; then echo "could not read tune2fs output"; exit 1; fi
awk -v bs="$BS" -v bc="$BC" -v fb="$FB" -v rb="$RB" 'BEGIN{
  printf "Volume size  = %d blocks x %d = %d bytes = %.2f GiB\n", bc, bs, bc*bs, bc*bs/1073741824
  printf "Free space   = %d blocks x %d = %d bytes = %.2f GiB\n", fb, bs, fb*bs, fb*bs/1073741824
  printf "Reserved     = %d blocks x %d = %d bytes = %.2f GiB\n", rb, bs, rb*bs, rb*bs/1073741824
  printf "Free - reserved = %d bytes (should be close to the Avail column of df)\n", (fb-rb)*bs }'
echo "df reports Avail (bytes):"
df -B1 --output=avail . | tail -n 1
echo "Inodes used  = $IC - $FI = $((IC - FI)) (compare with IUsed in df -i above)"
echo "Bytes of volume per inode = $((BC * BS / IC))"
