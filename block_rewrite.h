#ifndef BLOCK_REWRITE_H
#define BLOCK_REWRITE_H

#include "superblock_def.h"
#include "file_records.h"
#include "layout_plan.h"

typedef struct {
	int old_index; /* old data-region block index */
	int new_index; /* new data-region block index */
	int is_pointer; /* 1 if this entry maps a pointer block; 0 if data block */
} BlockMapEntry;

typedef struct {
	const struct superblock *sb;
	const unsigned char *in_buf;
	unsigned char *out_buf;
	const FileRecord *records;
	const FilePlacement *placements;
	int count; /* number of files */
	BlockMapEntry *map; /* dynamic array of mappings */
	int map_size;
} RewriteContext;

int build_block_mapping(RewriteContext *ctx); /* enumerate pointer+data blocks and fill map */
int rewrite_inodes(RewriteContext *ctx);      /* update inode pointers to new indices */
int rewrite_pointer_blocks(RewriteContext *ctx); /* copy pointer blocks with remapped entries */
int rewrite_data_blocks(RewriteContext *ctx); /* copy file payload blocks */

#endif /* BLOCK_REWRITE_H */
