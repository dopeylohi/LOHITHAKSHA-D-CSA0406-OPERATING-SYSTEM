#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

static int lib_mapped(void)
{
    FILE *f = fopen("/proc/self/maps", "r");
    char line[512];
    int found = 0;
    if (!f) return 0;
    while (fgets(line, sizeof line, f))
        if (strstr(line, "libbankenc")) { found = 1; fputs(line, stdout); }
    fclose(f);
    return found;
}

static void show_rss(void)
{
    FILE *f = fopen("/proc/self/status", "r");
    char line[256];
    if (!f) return;
    while (fgets(line, sizeof line, f))
        if (!strncmp(line, "VmRSS", 5)) fputs(line, stdout);
    fclose(f);
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "none";
    const char *path = argc > 2 ? argv[2] : "./libbankenc.so";
    printf("mode: %s\n", mode);
    if (!strcmp(mode, "enc")) {
        void *h = dlopen(path, RTLD_NOW);
        if (!h) { fprintf(stderr, "ERROR: cannot load encryption library: %s\n", dlerror()); return 2; }
        const char *(*ver)(void);
        void (*enc)(const char *, char *, size_t);
        *(void **)(&ver) = dlsym(h, "bankenc_version");
        *(void **)(&enc) = dlsym(h, "bankenc_encrypt");
        if (!ver || !enc) { fprintf(stderr, "ERROR: symbol missing: %s\n", dlerror()); dlclose(h); return 3; }
        char out[64], msg[] = "ACC1234-BALANCE-5000";
        enc(msg, out, sizeof out - 1);
        printf("library version : %s\ncipher (hex)    : ", ver());
        for (size_t i = 0; out[i]; i++) printf("%02x", (unsigned char)out[i]);
        printf("\n&bankenc_encrypt = %p (found by dlsym)\n", *(void **)(&enc));
        printf("library mapped  : %s\n", lib_mapped() ? "YES" : "no");
        show_rss();
        dlclose(h);
    } else {
        printf("no encryption needed, library not loaded\n");
        printf("library mapped  : %s\n", lib_mapped() ? "YES" : "no");
        show_rss();
    }
    return 0;
}
