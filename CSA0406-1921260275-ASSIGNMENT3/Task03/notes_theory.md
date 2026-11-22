# Task03 - Segmentation: written part ([T] = textbook fact)

## 1. Translation with base and limit
A logical address is (segment s, offset d). The segment table has a base and a limit for every segment. If d < limit[s], physical = base[s] + d; otherwise the hardware traps to the OS (addressing error). A segment number beyond the table length also traps. Implemented in seg.c (results in notes.md, Section 1).

## 2. Segmentation with paging (Intel x86, 32-bit protected mode) - Figure 3.1

    logical address = segment selector (16 bit) : offset (32 bit)
             |
             v
    selector -> descriptor table (GDT/LDT) -> descriptor: base, limit, access rights
             |
             v   check offset <= limit and rights, else fault
    linear address = base + offset (32 bit)
             |
             v
    | directory (10 bit) | page table (10 bit) | offset (12 bit) |
             |                    |
     CR3 -> page directory -> page table -> frame number
             |
             v
    physical address = frame * 4096 + offset (12 bit)

Segmentation gives separate, protected pieces (code, data, stack); paging then places each piece in any free frames, so there is no external fragmentation. Linux on x86 uses "flat" segments (base 0, limit = whole address space) and lets paging do the work; in 64-bit mode segment bases are ignored except FS and GS (thread-local data) [T].

## 3. Segmentation matches the programmer's view
A programmer thinks in named parts: main program, functions, global data, heap, stack, libraries. Each is a segment of its own length, addressed as (segment, offset). Paging cuts memory into equal blocks that do not match these parts, so one page can hold half of the code and half of the data, which need different protection.

## 4. Protection and sharing
- Each segment-table entry has permission bits (read / write / execute) and a limit. Code is read + execute, data read + write; writing into code, or an offset past the limit, traps (measured: offset 800 in a segment of limit 800 trapped).
- Sharing: two processes put the same base and limit in their segment tables, for example one read-only copy of the compiler's code segment shared by all students.

## 5. External fragmentation caused by segmentation
Segments have different lengths and need contiguous memory. After segments are loaded and removed, free memory is split into holes, and a new segment may not fit in any single hole although the total free memory is large. In my test the mean external fragmentation was 43.9% over 5 seeds [M] (notes.md, Section 4). Remedies: compaction (copy segments and update the bases, which is costly), or segmented paging.

## 6. Comparison of the three schemes
| | Pure paging | Pure segmentation | Segmented paging |
|---|---|---|---|
| Programmer's view | hidden, one linear space | segments visible | segments visible |
| External fragmentation | none | yes (43.9% in my test [M]) | none |
| Internal fragmentation | yes, about half a page per process or region | none | yes, about half a page per segment |
| Table size, my example [M] | 12,288 B two-level, 4,194,304 B single-level | 24 B | 196 B |
| Translation | 1 table lookup (or one per level) | 1 add + 1 compare | segment step, then page walk |
| Sharing and protection | per page, no natural unit | per segment, natural | per segment (page tables can be shared) |
| Allocation | any free frame | needs a big enough hole | any free frame |

## 7. Why modern operating systems rely mainly on paging
1. No external fragmentation: any free frame can be used and no compaction is needed.
2. Simple, fast allocation: all frames are the same size, so there is no search for a big enough hole.
3. Virtual memory works naturally: a page is the unit moved to and from disk; moving whole variable-size segments is slow.
4. The TLB makes paging fast, and multi-level tables keep tables small.
5. Protection and sharing are still available per page (permission bits, shared frames). Linux keeps the program's "segments" as memory areas (code, data, heap, stack) in software and maps them with pages; /proc/PID/maps showed these areas with different permissions [M].
The cost is internal fragmentation (average half a page per area) and page-table memory.

## 8. Alternative: pure paging instead of segmentation
- Solution: treat the program as one linear address space cut into 4 KB pages. Code, data and stack are ranges of pages with permission bits in each page-table entry (code r-x, data rw-); the OS keeps track of which ranges are which (Linux memory areas).
- Fragmentation: no external fragmentation. Internal fragmentation of about half a page per region: for my 20 segments with 4 KB pages it was 23 to 32 KB in total [M], against 43.9% external fragmentation for segmentation [M].
- Table size [M, C]: 24 B for segmentation, 196 B for segmented paging, 12,288 B for two-level paging and 4,194,304 B for single-level paging over the 32-bit space. Paging needs much larger tables.
- Translation time [T]: segmentation = 1 addition + 1 comparison, no memory access if the table is in registers (O(1)). Paging = 0 extra memory accesses on a TLB hit; on a miss 1 access for a single-level table or one per level (4 on x86-64). Segmented paging adds the segment step in front of the page walk. I did not measure translation time.
- Sharing and protection: possible per page, but a shared region is many entries that all have to be set, while a segment shares with one entry; with paging the OS has to remember the grouping.
