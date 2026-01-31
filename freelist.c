#include "freelist.h"
#include "inode_scan.h"
#include <string.h>
#include <stdlib.h>

int rebuild_free_block_list(unsigned char *out_buf,
                            const struct superblock *sb,
                            int head_start,
                            int total_data_blocks) {
    if (!out_buf || !sb) return -1;
    if (head_start < 0 || head_start > total_data_blocks) return -1;
    size_t data_base = 512 + 512 + (size_t)sb->data_offset * (size_t)sb->blocksize;
    /* For blocks from head_start to last, set first 4 bytes to next index, rest zeros */
    for (int idx = head_start; idx < total_data_blocks; ++idx) {
        size_t abs = data_base + (size_t)idx * (size_t)sb->blocksize;
        unsigned char *dst = out_buf + abs;
        int next = (idx + 1 < total_data_blocks) ? (idx + 1) : -1;
        /* Write little-endian next */
        dst[0] = (unsigned char)(next & 0xFF);
        dst[1] = (unsigned char)((next >> 8) & 0xFF);
        dst[2] = (unsigned char)((next >> 16) & 0xFF);
        dst[3] = (unsigned char)((next >> 24) & 0xFF);
        /* Zero remainder of block */
        memset(dst + 4, 0, (size_t)sb->blocksize - 4);
    }
    return 0;
}

static int is_used_inode(int idx, const int *used, int n) {
    for (int i = 0; i < n; ++i) if (used[i] == idx) return 1;
    return 0;
}

int rebuild_free_inode_list(unsigned char *out_buf,
                            const struct superblock *sb,
                            const int *used_inodes,
                            int used_count,
                            int total_inodes) {
    if (!out_buf || !sb) return -1;
    if (total_inodes <= 0) return -1;
    size_t inode_base = 512 + 512 + (size_t)sb->inode_offset * (size_t)sb->blocksize;
    /* Build a sorted list of free inode indices starting from sb->free_inode */
    int *free_list = (int *)malloc(sizeof(int) * (size_t)total_inodes);
    if (!free_list) return -1;
    int fcnt = 0;
    for (int i = 0; i < total_inodes; ++i) {
        if (!is_used_inode(i, used_inodes, used_count)) {
            free_list[fcnt++] = i;
        }
    }
    /* Ensure head is sb->free_inode by rotating list so it starts at that index if present */
    int head = sb->free_inode;
    int start_pos = 0;
    for (int i = 0; i < fcnt; ++i) { if (free_list[i] == head) { start_pos = i; break; } }
    /* Write next_inode fields: for each inode, set next to next free or -1; used inodes -> -1 */
    /* Only rewrite the chain reachable from head (sb->free_inode). Leave others untouched. */
    if (fcnt > 0) {
        for (int k = 0; k < fcnt; ++k) {
            int i = free_list[(start_pos + k) % fcnt];
            /* compute next in chain */
            int next = (k + 1 < fcnt) ? free_list[(start_pos + k + 1) % fcnt] : -1;
            size_t off = inode_base + (size_t)i * (size_t)sizeof(struct inode);
            out_buf[off + 0] = (unsigned char)(next & 0xFF);
            out_buf[off + 1] = (unsigned char)((next >> 8) & 0xFF);
            out_buf[off + 2] = (unsigned char)((next >> 16) & 0xFF);
            out_buf[off + 3] = (unsigned char)((next >> 24) & 0xFF);
        }
    }
    free(free_list);
    return 0;
}
