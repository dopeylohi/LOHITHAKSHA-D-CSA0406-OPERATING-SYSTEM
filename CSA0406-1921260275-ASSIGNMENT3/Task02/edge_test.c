#include <stdio.h>
#define PAGE_SIZE 1024
#define PAGES 8
int page_table[PAGES] = {5, 2, -1, 7, 0, -1, 3, -1};
int main(void)
{
 /* first and last byte of pages 1 and 6, last byte of page 7 (not in memory), first byte beyond the table */
 unsigned logical[] = {1024, 2047, 6144, 7167, 8191, 8192};
 for (int i = 0; i < 6; i++) {
 unsigned a = logical[i];
 unsigned p = a / PAGE_SIZE;
 unsigned d = a % PAGE_SIZE;
 printf("%u -> page %u offset %u : ", a, p, d);
 if (p >= PAGES) printf("invalid address\n");
 else if (page_table[p] == -1) printf("page fault\n");
 else printf("physical %u\n", (unsigned)page_table[p] * PAGE_SIZE + d);
 }
 return 0;
}
