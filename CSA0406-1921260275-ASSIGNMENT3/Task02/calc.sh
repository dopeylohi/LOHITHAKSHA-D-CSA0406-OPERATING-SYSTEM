#!/bin/bash
# Q2 calculations with working shown. [C] = calculated, [A] = assumed.
echo "=== A. Page-table size, single-level, 4 KB pages [C] ==="
echo "32-bit: entries = 2^(32-12) = $((1<<20)); entry 4 bytes [A] -> $(( (1<<20)*4 )) bytes = $(( (1<<20)*4/1024/1024 )) MiB per process"
echo "48-bit: entries = 2^(48-12) = $((1<<36)); entry 8 bytes [A] -> $(( (1<<36)*8 )) bytes = $(( (1<<36)*8/1024/1024/1024 )) GiB per process"
echo "48-bit, 4-level (9 bits per level, 4 KB tables) for a process touching one page: 4 tables x 4 KB = $((4*4)) KB minimum [C]"
echo
echo "=== B. Table size for different page sizes ==="
for ps in 1024 4096 2097152; do
  e32=$(( (1<<32)/ps )); e48=$(( (1<<48)/ps ))
  echo "page $ps B: 32-bit entries $e32 -> $((e32*4/1024)) KiB (4 B/entry) ; 48-bit entries $e48 -> $((e48*8/1024/1024/1024)) GiB (8 B/entry)"
done
echo
echo "=== C. Effective access time (EAT), memory m=100 ns, TLB t=10 ns [A] ==="
echo "Formula: EAT = h*(t+m*L) + (1-h)*(t+m*(L+1)), where L = 1 for one-level (hit: t+m; miss: t+2m)"
awk 'BEGIN{
  m=100; t=10;
  printf "No TLB, one-level table : 2m = %d ns\n", 2*m;
  printf "No TLB, 4-level table   : 5m = %d ns\n", 5*m;
  split("0.80 0.95 0.99", H, " ");
  for (i=1;i<=3;i++){ h=H[i];
    e1 = h*(t+m) + (1-h)*(t+2*m);
    e4 = h*(t+m) + (1-h)*(t+5*m);
    printf "h=%.2f : one-level = %.2f*%d + %.2f*%d = %.1f ns ; 4-level = %.2f*%d + %.2f*%d = %.1f ns\n", h,h,t+m,1-h,t+2*m,e1, h,t+m,1-h,t+5*m,e4 }
}'
echo
echo "=== D. Inverted page table size (alternative) ==="
echo "RAM 16 GB [A], page 4 KB -> frames = $(( (16<<30)/4096 )); entry 16 bytes [A] -> $(( (16<<30)/4096*16/1024/1024 )) MiB for the whole machine, independent of the number of processes [C]"
