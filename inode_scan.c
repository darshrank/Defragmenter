#include "inode_scan.h"
#include "util.h"
#include <string.h>

/* Compute absolute byte offsets for regions based on superblock */
static void compute_region_bounds(const struct superblock *sb,
                                  size_t *inode_start,
                                  size_t *data_start,
                                  size_t *swap_start) {
    /* Boot block: 512, Super block: 512; offsets are in blocks from end of super block */
    size_t base = 512 /* boot */ + 512 /* super */;
    *inode_start = base + (size_t)sb->inode_offset * (size_t)sb->blocksize;
    *data_start  = base + (size_t)sb->data_offset  * (size_t)sb->blocksize;
    *swap_start  = base + (size_t)sb->swap_offset  * (size_t)sb->blocksize;
}

int scan_inodes(const unsigned char *buf, const struct superblock *sb, InodeView *array, int *count) {
    if (!buf || !sb || !count) return -1;

    size_t inode_start = 0, data_start = 0, swap_start = 0;
    compute_region_bounds(sb, &inode_start, &data_start, &swap_start);

    size_t inode_region_bytes = (data_start > inode_start) ? (data_start - inode_start) : 0;
    if (inode_region_bytes == 0) {
        *count = 0;
        return 0;
    }

    const size_t inode_size = sizeof(struct inode); /* expected 100 */
    int capacity = (int)(inode_region_bytes / inode_size);
    int used_count = 0;

    for (int idx = 0; idx < capacity; ++idx) {
        size_t off = inode_start + (size_t)idx * inode_size;
        const struct inode *in = (const struct inode *)(buf + off);
        /* An inode is considered used if nlink > 0, per README */
        if (in->nlink > 0) {
            if (array) {
                array[used_count].inode_index = idx;
                array[used_count].size_bytes = in->size;
                array[used_count].raw = in;
            }
            used_count++;
        }
    }

    *count = used_count;
    return 0;
}
