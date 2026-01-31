#ifndef FREELIST_H
#define FREELIST_H

#include "superblock_def.h"

/* Rebuild ascending free block list in data region starting at head_start.
	total_data_blocks is the number of blocks in the data region.
	Returns 0 on success. */
int rebuild_free_block_list(unsigned char *out_buf,
									 const struct superblock *sb,
									 int head_start,
									 int total_data_blocks);

/* Rebuild free inode linked list by setting next_inode for free inodes.
	total_inodes is count of inode slots in inode region.
	used_inodes is an array of indices of used inodes (length used_count).
	Updates next_inode fields in out_buf inode region. */
int rebuild_free_inode_list(unsigned char *out_buf,
									 const struct superblock *sb,
									 const int *used_inodes,
									 int used_count,
									 int total_inodes);

#endif /* FREELIST_H */
