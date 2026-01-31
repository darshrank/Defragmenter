#include "disk_image.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include "util.h"
#include "superblock_def.h"

/* Load entire disk image into memory */
int load_disk_image(const char *path, unsigned char **buffer, size_t *size) {
    struct stat st;
    if (stat(path, &st) != 0) {
        fatal("Cannot stat input image '%s'", path);
    }
    size_t fsize = (size_t)st.st_size;
    FILE *f = fopen(path, "rb");
    if (!f) {
        fatal("Cannot open input image '%s'", path);
    }
    unsigned char *buf = (unsigned char *)malloc(fsize);
    if (!buf) {
        fclose(f);
        fatal("malloc failed for %zu bytes", fsize);
    }
    size_t rd = fread(buf, 1, fsize, f);
    fclose(f);
    if (rd != fsize) {
        free(buf);
        fatal("Short read: expected %zu got %zu", fsize, rd);
    }
    *buffer = buf;
    *size = fsize;
    return 0;
}

/* Parse superblock located immediately after boot block (offset 512) */
int parse_superblock(const unsigned char *buf, struct superblock *out) {
    if (!buf || !out) return -1;
    const size_t sb_offset = 512; /* boot block is first 512 bytes */
    out->blocksize    = safe_read_int_le(buf + sb_offset + 0);
    out->inode_offset = safe_read_int_le(buf + sb_offset + 4);
    out->data_offset  = safe_read_int_le(buf + sb_offset + 8);
    out->swap_offset  = safe_read_int_le(buf + sb_offset + 12);
    out->free_inode   = safe_read_int_le(buf + sb_offset + 16);
    out->free_block   = safe_read_int_le(buf + sb_offset + 20);
    /* Basic sanity checks */
    if (out->blocksize <= 0) {
        fatal("Invalid blocksize %d", out->blocksize);
    }
    return 0;
}

int write_disk_image(const char *path, const unsigned char *buffer, size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t wr = fwrite(buffer, 1, size, f);
    fclose(f);
    return wr == size ? 0 : -1;
}
