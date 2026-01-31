#ifndef FILE_RECORDS_H
#define FILE_RECORDS_H

#include <stddef.h>
#include "superblock_def.h"
#include "inode_scan.h"

typedef struct {
    int inode_index;
    int size_bytes;
    int data_block_count;     /* number of payload blocks used */
    const struct inode *raw;  /* pointer to original inode */
    /* Arrays of block indices relative to data region */
    int direct_blocks[N_DBLOCKS];
    int direct_count;
    int pointer_blocks_count; /* exact pointer blocks needed */
    /* Note: indirect pointers will be resolved later when rewriting */
} FileRecord;

/* Build FileRecord array from used inodes. Allocates records; caller frees via free_file_records. */
int build_file_records(const unsigned char *buf,
                       const struct superblock *sb,
                       const InodeView *inodes,
                       int inode_count,
                       FileRecord **out_records,
                       int *out_count);

void free_file_records(FileRecord *records, int count);

#endif /* FILE_RECORDS_H */
