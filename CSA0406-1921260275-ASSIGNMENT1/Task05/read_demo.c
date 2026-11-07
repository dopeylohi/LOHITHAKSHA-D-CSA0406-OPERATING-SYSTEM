#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int main(void)
{
    char buf[64];
    int fd = open("rec.txt", O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    ssize_t n = read(fd, buf, sizeof buf);
    if (n > 0 && write(1, buf, n) < 0) return 1;
    close(fd);
    return 0;
}
