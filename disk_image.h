#ifndef DISK_IMAGE_H
#define DISK_IMAGE_H

#include <stddef.h>

/* Forward declarations */
struct superblock;

typedef struct {
    unsigned char *buffer; /* raw image buffer */
    size_t size;           /* total size in bytes */
} DiskImage;

int load_disk_image(const char *path, unsigned char **buffer, size_t *size); /* returns 0 on success */
int parse_superblock(const unsigned char *buf, struct superblock *out);      /* 0 on success */
int write_disk_image(const char *path, const unsigned char *buffer, size_t size); /* 0 on success */

#endif /* DISK_IMAGE_H */
