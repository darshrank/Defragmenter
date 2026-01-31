#include "layout_plan.h"

int plan_layout(const struct superblock *sb,
                const FileRecord *records,
                int record_count,
                FilePlacement *out,
                int *out_count,
                int *next_free_start) {
    if (!sb || !records || record_count < 0 || !out || !out_count || !next_free_start) return -1;
    int cursor = 0; /* start of data region after defrag */
    for (int i = 0; i < record_count; ++i) {
        int pointer_blocks = records[i].pointer_blocks_count;
        out[i].inode_index = records[i].inode_index;
        out[i].start_block = cursor;
        out[i].pointer_block_count = pointer_blocks;
        out[i].data_block_count = records[i].data_block_count;
        cursor += pointer_blocks + records[i].data_block_count;
    }
    *out_count = record_count;
    *next_free_start = cursor; /* first free block index after all files */
    return 0;
}
