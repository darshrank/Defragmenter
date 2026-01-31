#include "file_records.h"
#include <stdlib.h>
#include <string.h>

static int ceil_div(int a, int b) {
    return (a + b - 1) / b;
}

int build_file_records(const unsigned char *buf,
                       const struct superblock *sb,
                       const InodeView *inodes,
                       int inode_count,
                       FileRecord **out_records,
                       int *out_count) {
    (void)buf; /* buffer may be used in future for indirect analysis */
    if (!sb || !inodes || inode_count < 0 || !out_records || !out_count) return -1;

    FileRecord *recs = (FileRecord *)calloc((size_t)inode_count, sizeof(FileRecord));
    if (!recs) return -1;

    for (int i = 0; i < inode_count; ++i) {
        const InodeView *iv = &inodes[i];
        FileRecord *fr = &recs[i];
        fr->inode_index = iv->inode_index;
        fr->size_bytes = iv->size_bytes;
        fr->raw = iv->raw;

        int total_blocks = ceil_div(iv->size_bytes, sb->blocksize);
        if (total_blocks < 0) total_blocks = 0;
        fr->data_block_count = total_blocks;

        /* Collect direct blocks up to min(N_DBLOCKS, total_blocks) */
        int want = total_blocks < N_DBLOCKS ? total_blocks : N_DBLOCKS;
        fr->direct_count = 0;
        for (int j = 0; j < want; ++j) {
            fr->direct_blocks[j] = iv->raw->dblocks[j];
            fr->direct_count++;
        }
        /* Estimate exact pointer blocks needed by consuming available levels */
        int remaining = total_blocks - fr->direct_count;
        int per_block = sb->blocksize / 4;
        int ptr_blocks = 0;
        /* Single indirect: consume as many iblocks entries as available and needed */
        if (remaining > 0) {
            for (int ib = 0; ib < N_IBLOCKS && remaining > 0; ++ib) {
                if (iv->raw->iblocks[ib] == -1) break;
                int use_single = remaining < per_block ? remaining : per_block;
                ptr_blocks += 1; /* this single-indirect pointer block */
                remaining -= use_single;
            }
        }
        /* Double indirect */
        if (remaining > 0 && iv->raw->i2block != -1) {
            int single_capacity = per_block;
            int double_capacity = per_block * single_capacity;
            int use_double = remaining < double_capacity ? remaining : double_capacity;
            int needed_single_blocks = (use_double + single_capacity - 1) / single_capacity;
            ptr_blocks += 1; /* double root */
            ptr_blocks += needed_single_blocks; /* singles under double */
            remaining -= use_double;
        }
        /* Triple indirect */
        if (remaining > 0 && iv->raw->i3block != -1) {
            int single_capacity = per_block;
            int double_capacity = per_block * single_capacity;
            int triple_capacity = per_block * double_capacity;
            int use_triple = remaining < triple_capacity ? remaining : triple_capacity;
            int needed_double_blocks = (use_triple + double_capacity - 1) / double_capacity;
            int needed_single_blocks = 0;
            int rem_triple = use_triple;
            for (int d = 0; d < needed_double_blocks; ++d) {
                int portion = rem_triple < double_capacity ? rem_triple : double_capacity;
                int singles_for_d = (portion + single_capacity - 1) / single_capacity;
                needed_single_blocks += singles_for_d;
                rem_triple -= portion;
            }
            ptr_blocks += 1; /* triple root */
            ptr_blocks += needed_double_blocks;
            ptr_blocks += needed_single_blocks;
            remaining -= use_triple;
        }
        fr->pointer_blocks_count = ptr_blocks;
    }

    *out_records = recs;
    *out_count = inode_count;
    return 0;
}

void free_file_records(FileRecord *records, int count) {
    (void)count;
    free(records);
}
