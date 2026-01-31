#include "util.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

int safe_read_int_le(const unsigned char *p) {
    /* Little-endian 4-byte integer */
    return (int)(
        ((unsigned int)p[0]) |
        ((unsigned int)p[1] << 8) |
        ((unsigned int)p[2] << 16) |
        ((unsigned int)p[3] << 24)
    );
}

void fatal(const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "Error: ");
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}
