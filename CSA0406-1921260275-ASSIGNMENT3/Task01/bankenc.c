#include "bankenc.h"
/* Toy "encryption library" for the bank. Build with -DV2 for the security-fixed version. */
#ifdef V2
#define KEY 0x7C
#define VER "2.0 (security fix, key 0x7C)"
#else
#define KEY 0x5A
#define VER "1.0 (original, key 0x5A)"
#endif
const char *bankenc_version(void) { return VER; }
void bankenc_encrypt(const char *in, char *out, size_t n)
{
    size_t i;
    for (i = 0; i < n && in[i] != '\0'; i++)
        out[i] = (char)(in[i] ^ KEY);
    out[i] = '\0';
}
