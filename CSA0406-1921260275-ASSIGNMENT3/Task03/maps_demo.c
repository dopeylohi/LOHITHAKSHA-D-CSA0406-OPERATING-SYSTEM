#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int g_data = 42;            /* initialised data  */
int g_bss;                  /* uninitialised data */
const char g_ro[] = "read-only constant";
static void code_marker(void) {}
int main(void)
{
    int local = 1;
    char *heap = malloc(1000);
    char line[512];
    printf("PID %d\n", (int)getpid());
    printf("code  (function)   %p\n", (void *)code_marker);
    printf("rodata (constant)  %p\n", (const void *)g_ro);
    printf("data  (g_data)     %p\n", (void *)&g_data);
    printf("bss   (g_bss)      %p\n", (void *)&g_bss);
    printf("heap  (malloc)     %p\n", (void *)heap);
    printf("stack (local var)  %p\n", (void *)&local);
    printf("--- /proc/%d/maps ---\n", (int)getpid());
    FILE *f = fopen("/proc/self/maps", "r");
    if (!f) { perror("maps"); return 1; }
    while (fgets(line, sizeof line, f)) fputs(line, stdout);
    fclose(f);
    free(heap);
    return 0;
}
