#ifndef INODE_SCAN_H
#define INODE_SCAN_H

#include <stddef.h>
#include "superblock_def.h"

#define N_DBLOCKS 10
#define N_IBLOCKS 4

struct inode {
    int next_inode;
    int protect;
    int nlink;
    int size;
    int uid;
    int gid;
    int ctime;
    int mtime;
    int atime;
    int dblocks[N_DBLOCKS];
    int iblocks[N_IBLOCKS];
    int i2block;
    int i3block;
};

typedef struct {
    int inode_index;
    int size_bytes;
    const struct inode *raw;
} InodeView;

int scan_inodes(const unsigned char *buf, const struct superblock *sb, InodeView *array, int *count); /* 0 success */

#endif /* INODE_SCAN_H */
