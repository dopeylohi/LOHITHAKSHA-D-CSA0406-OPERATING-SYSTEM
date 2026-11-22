#!/bin/bash
# Q3: table sizes in each scheme for one small program [A]. [C] = calculated.
code=$((100*1024)); data=$((50*1024)); stack=$((20*1024)); pg=4096
pc=$(( (code+pg-1)/pg )); pd=$(( (data+pg-1)/pg )); ps=$(( (stack+pg-1)/pg ))
echo "Program [A]: code 100 KB, data 50 KB, stack 20 KB; 32-bit addresses; 4 KB pages; table entry 4 B, segment entry 8 B (base+limit) [A]"
echo "Pages needed: code $pc, data $pd, stack $ps, total $((pc+pd+ps))"
echo "1. Pure segmentation : 3 segments x 8 B = $((3*8)) bytes"
echo "2. Pure paging, single-level, whole 32-bit space : 2^20 entries x 4 B = $(( (1<<20)*4 )) bytes ($(( (1<<20)*4/1024 )) KiB)"
echo "3. Pure paging, two-level [A: code+data share one 2nd-level table, stack in another]: 1 top + 2 second-level tables x 4096 B = $((3*4096)) bytes"
echo "4. Segmented paging  : 3 segment entries x 8 B + $((pc+pd+ps)) page-table entries x 4 B = $((3*8)) + $(( (pc+pd+ps)*4 )) = $((3*8 + (pc+pd+ps)*4)) bytes"
