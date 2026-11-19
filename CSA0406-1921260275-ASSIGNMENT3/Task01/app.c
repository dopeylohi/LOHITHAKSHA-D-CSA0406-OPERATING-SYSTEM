#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "bankenc.h"

static void show_mem(void)
{
    FILE *f = fopen("/proc/self/status", "r");
    char line[256];
    if (!f) return;
    while (fgets(line, sizeof line, f))
        if (!strncmp(line, "VmSize", 6) || !strncmp(line, "VmRSS", 5)) fputs(line, stdout);
    fclose(f);
}

static void show_maps(void)
{
    FILE *f = fopen("/proc/self/maps", "r");
    char line[512];
    if (!f) return;
    puts("--- /proc/self/maps (executable segments, heap, stack) ---");
    while (fgets(line, sizeof line, f))
        if (strstr(line, "r-xp") || strstr(line, "[heap]") || strstr(line, "[stack]"))
            fputs(line, stdout);
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "full";
    char out[64], msg[] = "ACC1234-BALANCE-5000";
    bankenc_encrypt(msg, out, sizeof out - 1);
    if (!strcmp(mode, "q")) return 0;
    if (!strcmp(mode, "mem")) { show_mem(); return 0; }
    printf("library version : %s\n", bankenc_version());
    printf("plain text      : %s\n", msg);
    printf("cipher (hex)    : ");
    for (size_t i = 0; out[i]; i++) printf("%02x", (unsigned char)out[i]);
    printf("\n&main          = %p\n&bankenc_encrypt = %p\n", (void *)main, (void *)bankenc_encrypt);
    show_mem();
    show_maps();
    if (!strcmp(mode, "wait")) { fflush(stdout); sleep(6); }
    return 0;
}
