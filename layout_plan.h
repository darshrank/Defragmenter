#ifndef LAYOUT_PLAN_H
#define LAYOUT_PLAN_H

#include <stddef.h>
#include "file_records.h"
#include "superblock_def.h"

typedef struct {
    int inode_index;         /* inode id */
    int start_block;         /* first new block index for this file (pointer blocks first) */
    int pointer_block_count; /* how many pointer blocks reserved */
    int data_block_count;    /* how many data blocks (payload) */
} FilePlacement;

/* Compute contiguous layout for files; returns total blocks consumed (excluding free list). */
int plan_layout(const struct superblock *sb,
                const FileRecord *records,
                int record_count,
                FilePlacement *out,
                int *out_count,
                int *next_free_start);

#endif /* LAYOUT_PLAN_H */
