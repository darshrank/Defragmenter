#include "block_rewrite.h"
#include "inode_scan.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

static void map_add(BlockMapEntry **map, int *size, int old_idx, int new_idx, int is_pointer) {
    BlockMapEntry *m = *map;
    int n = *size;
    m = (BlockMapEntry *)realloc(m, sizeof(BlockMapEntry) * (size_t)(n + 1));
    if (!m) fatal("realloc failed for block map");
    m[n].old_index = old_idx;
    m[n].new_index = new_idx;
    m[n].is_pointer = is_pointer;
    *map = m;
    *size = n + 1;
}

static int map_lookup(const RewriteContext *ctx, int old_idx) {
    for (int i = 0; i < ctx->map_size; ++i) {
        if (ctx->map[i].old_index == old_idx) return ctx->map[i].new_index;
    }
    return -1;
}

int build_block_mapping(RewriteContext *ctx) {
    if (!ctx || !ctx->sb || !ctx->records || !ctx->placements) return -1;
    ctx->map = NULL;
    ctx->map_size = 0;

    int ptrs_per_block = ctx->sb->blocksize / 4;

    for (int i = 0; i < ctx->count; ++i) {
        const FileRecord *fr = &ctx->records[i];
        const FilePlacement *pl = &ctx->placements[i];
          int cursor = pl->start_block;

        /* Map direct data blocks */
        for (int j = 0; j < fr->direct_count; ++j) {
            int old = fr->direct_blocks[j];
            int newi = cursor;
            map_add(&ctx->map, &ctx->map_size, old, newi, 0);
            cursor++;
        }

        int remaining = fr->data_block_count - fr->direct_count;
        int data_offset = fr->direct_count;
        /* After direct blocks, place single-indirect pointer blocks followed by their data */

        /* Consume single-indirect blocks available in iblocks array before using double/triple */
        if (remaining > 0) {
            size_t base = 512 + 512 + (size_t)ctx->sb->data_offset * (size_t)ctx->sb->blocksize;
            for (int ib = 0; ib < N_IBLOCKS && remaining > 0; ++ib) {
                int single_block_old = fr->raw->iblocks[ib];
                if (single_block_old == -1) break;
                int single_block_new = cursor;
                map_add(&ctx->map, &ctx->map_size, single_block_old, single_block_new, 1);
                cursor++;
                size_t single_abs = base + (size_t)single_block_old * (size_t)ctx->sb->blocksize;
                const unsigned char *p = ctx->in_buf + single_abs;
                for (int k = 0; k < ptrs_per_block && remaining > 0; ++k) {
                    int old_data_idx = safe_read_int_le(p + (size_t)k * 4);
                    if (old_data_idx == -1) break;
                    int new_data_idx = cursor;
                    map_add(&ctx->map, &ctx->map_size, old_data_idx, new_data_idx, 0);
                    cursor++;
                    remaining--;
                }
            }
        }

        /* Double indirect enumeration */
        if (remaining > 0 && fr->raw->i2block != -1) {
            int dind_old = fr->raw->i2block;
            int dind_new = cursor;
            map_add(&ctx->map, &ctx->map_size, dind_old, dind_new, 1);
            cursor++;
            size_t base = 512 + 512 + (size_t)ctx->sb->data_offset * (size_t)ctx->sb->blocksize;
            size_t dind_abs = base + (size_t)dind_old * (size_t)ctx->sb->blocksize;
            const unsigned char *dp = ctx->in_buf + dind_abs;
            for (int k = 0; k < ptrs_per_block && remaining > 0; ++k) {
                int single_block_old = safe_read_int_le(dp + (size_t)k * 4);
                if (single_block_old == -1) break;
                int single_block_new = cursor;
                map_add(&ctx->map, &ctx->map_size, single_block_old, single_block_new, 1);
                cursor++;
                size_t single_abs = base + (size_t)single_block_old * (size_t)ctx->sb->blocksize;
                const unsigned char *sp = ctx->in_buf + single_abs;
                for (int s = 0; s < ptrs_per_block && remaining > 0; ++s) {
                    int old_data_idx = safe_read_int_le(sp + (size_t)s * 4);
                    if (old_data_idx == -1) break;
                    int new_data_idx = cursor;
                    map_add(&ctx->map, &ctx->map_size, old_data_idx, new_data_idx, 0);
                    cursor++;
                    remaining--;
                }
            }
        }

        /* Triple indirect enumeration */
        if (remaining > 0 && fr->raw->i3block != -1) {
            int tind_old = fr->raw->i3block;
            int tind_new = cursor;
            map_add(&ctx->map, &ctx->map_size, tind_old, tind_new, 1);
            cursor++;
            size_t base = 512 + 512 + (size_t)ctx->sb->data_offset * (size_t)ctx->sb->blocksize;
            size_t tind_abs = base + (size_t)tind_old * (size_t)ctx->sb->blocksize;
            const unsigned char *tp = ctx->in_buf + tind_abs;
            for (int k = 0; k < ptrs_per_block && remaining > 0; ++k) {
                int dind_old = safe_read_int_le(tp + (size_t)k * 4);
                if (dind_old == -1) break;
                int dind_new = cursor;
                map_add(&ctx->map, &ctx->map_size, dind_old, dind_new, 1);
                cursor++;
                size_t dind_abs = base + (size_t)dind_old * (size_t)ctx->sb->blocksize;
                const unsigned char *dp = ctx->in_buf + dind_abs;
                for (int d = 0; d < ptrs_per_block && remaining > 0; ++d) {
                    int single_block_old = safe_read_int_le(dp + (size_t)d * 4);
                    if (single_block_old == -1) break;
                    int single_block_new = cursor;
                    map_add(&ctx->map, &ctx->map_size, single_block_old, single_block_new, 1);
                    cursor++;
                    size_t single_abs = base + (size_t)single_block_old * (size_t)ctx->sb->blocksize;
                    const unsigned char *sp = ctx->in_buf + single_abs;
                    for (int s = 0; s < ptrs_per_block && remaining > 0; ++s) {
                        int old_data_idx = safe_read_int_le(sp + (size_t)s * 4);
                        if (old_data_idx == -1) break;
                        int new_data_idx = cursor;
                        map_add(&ctx->map, &ctx->map_size, old_data_idx, new_data_idx, 0);
                        cursor++;
                        remaining--;
                    }
                }
            }
        }

    }

    return 0;
}

int rewrite_inodes(RewriteContext *ctx) {
    if (!ctx || !ctx->out_buf || !ctx->sb) return -1;
    /* Compute region starts */
    size_t base = 512 + 512; /* after boot+super */
    size_t inode_start = base + (size_t)ctx->sb->inode_offset * (size_t)ctx->sb->blocksize;
    /* For each record, write updated pointers into inode slot */
    for (int i = 0; i < ctx->count; ++i) {
        const FileRecord *fr = &ctx->records[i];
        const FilePlacement *pl = &ctx->placements[i];
        size_t off = inode_start + (size_t)fr->inode_index * sizeof(struct inode);
        struct inode *out_inode = (struct inode *)(ctx->out_buf + off);
        /* Copy inode fields from input raw first */
        memcpy(out_inode, fr->raw, sizeof(struct inode));
        /* Rewrite direct blocks */
        for (int j = 0; j < N_DBLOCKS; ++j) {
            int old = fr->raw->dblocks[j];
            if (old == -1) { out_inode->dblocks[j] = -1; continue; }
            int mapped = map_lookup(ctx, old);
            out_inode->dblocks[j] = mapped;
        }
        /* Rewrite single indirect pointers (indices to pointer blocks) */
        for (int k = 0; k < N_IBLOCKS; ++k) {
            int oldp = fr->raw->iblocks[k];
            if (oldp == -1) { out_inode->iblocks[k] = -1; continue; }
            int mappedp = map_lookup(ctx, oldp);
            out_inode->iblocks[k] = mappedp;
        }
        /* Rewrite double and triple */
        if (fr->raw->i2block != -1) {
            out_inode->i2block = map_lookup(ctx, fr->raw->i2block);
        } else {
            out_inode->i2block = -1;
        }
        if (fr->raw->i3block != -1) {
            out_inode->i3block = map_lookup(ctx, fr->raw->i3block);
        } else {
            out_inode->i3block = -1;
        }
    }
    return 0;
}

int rewrite_pointer_blocks(RewriteContext *ctx) {
    if (!ctx || !ctx->in_buf || !ctx->out_buf || !ctx->sb) return -1;
    size_t data_base = 512 + 512 + (size_t)ctx->sb->data_offset * (size_t)ctx->sb->blocksize;
    int ptrs_per_block = ctx->sb->blocksize / 4;
    /* Iterate mapping entries; only rewrite pointer blocks */
    for (int m = 0; m < ctx->map_size; ++m) {
        if (ctx->map[m].is_pointer != 1) continue;
        int old_idx = ctx->map[m].old_index;
        int new_idx = ctx->map[m].new_index;
        /* Heuristic: pointer blocks are those that appear as mapped from inode pointer fields.
           We can't distinguish here; rewrite both pointer and data safely:
           For pointer blocks, entries are 4-byte ints; for data blocks, just raw copy.
           We'll attempt to rewrite as pointer block if its contents look like indices or -1. */
        size_t old_abs = data_base + (size_t)old_idx * (size_t)ctx->sb->blocksize;
        size_t new_abs = data_base + (size_t)new_idx * (size_t)ctx->sb->blocksize;
        const unsigned char *src = ctx->in_buf + old_abs;
        unsigned char *dst = ctx->out_buf + new_abs;
        /* Rewrite pointer block: map each int if not -1 */
        for (int i = 0; i < ptrs_per_block; ++i) {
            int val = safe_read_int_le(src + (size_t)i * 4);
            int outv = val;
            if (val != -1) {
                int mapped = map_lookup(ctx, val);
                if (mapped != -1) outv = mapped; else outv = val;
            }
            /* Write little-endian */
            dst[i*4 + 0] = (unsigned char)(outv & 0xFF);
            dst[i*4 + 1] = (unsigned char)((outv >> 8) & 0xFF);
            dst[i*4 + 2] = (unsigned char)((outv >> 16) & 0xFF);
            dst[i*4 + 3] = (unsigned char)((outv >> 24) & 0xFF);
        }
        /* Zero remainder beyond pointer array */
        memset(dst + (size_t)ptrs_per_block * 4, 0, (size_t)ctx->sb->blocksize - (size_t)ptrs_per_block * 4);
    }
    return 0;
}

int rewrite_data_blocks(RewriteContext *ctx) {
    if (!ctx || !ctx->in_buf || !ctx->out_buf || !ctx->sb) return -1;
    size_t data_base = 512 + 512 + (size_t)ctx->sb->data_offset * (size_t)ctx->sb->blocksize;
    /* Copy only data blocks using mapping; pointer blocks handled separately */
    for (int m = 0; m < ctx->map_size; ++m) {
        if (ctx->map[m].is_pointer == 1) continue;
        int old_idx = ctx->map[m].old_index;
        int new_idx = ctx->map[m].new_index;
        size_t old_abs = data_base + (size_t)old_idx * (size_t)ctx->sb->blocksize;
        size_t new_abs = data_base + (size_t)new_idx * (size_t)ctx->sb->blocksize;
        memcpy(ctx->out_buf + new_abs, ctx->in_buf + old_abs, (size_t)ctx->sb->blocksize);
    }
    return 0;
}
