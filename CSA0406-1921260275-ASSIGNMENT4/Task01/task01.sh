#!/bin/bash
# Task 1: calculations [C] with stated assumptions [A], and a real model of the improved design
cd "$(dirname "$0")" || { echo "cd failed"; exit 1; }

echo "=== 1.1 Search cost: flat directory vs improved tree (linear scan, average = n/2) ==="
STUDENTS=12000     # [A] given in the question
FILES=50           # [A] files per student
DEPTS=12           # [A] departments, equal size
BATCHES=4          # [A] batches per department
N=$((STUDENTS * FILES))
FLAT=$((N / 2))
PER_DEPT=$((STUDENTS / DEPTS))
PER_BATCH=$((PER_DEPT / BATCHES))
TREE=$((DEPTS / 2 + BATCHES / 2 + PER_BATCH / 2 + FILES / 2))
echo "Entries in flat directory N = $STUDENTS x $FILES = $N"
echo "Flat average comparisons    = N/2 = $FLAT"
echo "Tree: $DEPTS depts, $BATCHES batches, $PER_BATCH students per batch, $FILES files per student"
echo "Tree average comparisons    = $((DEPTS/2)) + $((BATCHES/2)) + $((PER_BATCH/2)) + $((FILES/2)) = $TREE"
echo "Flat / tree ratio           = $((FLAT / TREE)) times (integer division)"

echo
echo "=== 1.2 Model of the improved design (modes and links only) ==="
rm -rf -- demo
mkdir -p demo/students/IT/2026/1921260275 demo/students/IT/2026/1921260999 || exit 1
mkdir -p demo/projects/IT/P017 demo/courses/CSA0406/materials demo/courses/CSA0406/submit || exit 1
chmod 700  demo/students/IT/2026/1921260275 demo/students/IT/2026/1921260999
chmod 2770 demo/projects/IT/P017      # rwx for owner+group, setgid so files inherit the group
chmod 755  demo/courses/CSA0406/materials
chmod 1733 demo/courses/CSA0406/submit # drop-box: others may write, cannot list, sticky stops deleting others' files
ln -s ../../../../projects/IT/P017 demo/students/IT/2026/1921260275/P017
ln -s ../../../../projects/IT/P017 demo/students/IT/2026/1921260999/P017
find demo -printf '%M %4m  %p  %l\n' | sort -k3
echo
echo "Link check (both members reach the same single copy):"
readlink -f demo/students/IT/2026/1921260275/P017
readlink -f demo/students/IT/2026/1921260999/P017

echo
echo "=== 1.3 Defence word count (limit 100) ==="
wc -w < defence.txt
