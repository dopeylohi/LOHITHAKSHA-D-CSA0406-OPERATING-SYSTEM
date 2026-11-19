#ifndef BANKENC_H
#define BANKENC_H
#include <stddef.h>
const char *bankenc_version(void);
void bankenc_encrypt(const char *in, char *out, size_t n);
#endif
