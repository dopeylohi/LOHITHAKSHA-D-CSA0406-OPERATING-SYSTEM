#include <stdio.h>
#define PAGE_SIZE 2048
#define PAGES 8
/* Page table of a process: frame number, or -1 if the page is not in memory. */
int page_table[PAGES] = {5, 2, -1, 7, 0, -1, 3, -1};
int main(void)
{
 unsigned logical[] = {0, 1500, 3100, 6200, 9000, 2049};
 for (int i = 0; i < 6; i++) {
 unsigned a = logical[i];
 // TODO 1: Split a into page number p = a / PAGE_SIZE and offset d = a %
 // PAGE_SIZE. If p is outside the table print "invalid address"; if the frame
 // is -1 print "page fault"; otherwise print the physical address frame *
 // PAGE_SIZE + d
 unsigned p = a / PAGE_SIZE;
 unsigned d = a % PAGE_SIZE;
 printf("%u -> page %u offset %u : ", a, p, d);
 if (p >= PAGES) printf("invalid address\n");
 else if (page_table[p] == -1) printf("page fault\n");
 else printf("physical %u\n", (unsigned)page_table[p] * PAGE_SIZE + d);
 }
 return 0;
}
