#ifndef SUPERBLOCK_DEF_H
#define SUPERBLOCK_DEF_H

struct superblock {
    int blocksize;
    int inode_offset;
    int data_offset;
    int swap_offset;
    int free_inode;
    int free_block;
};

#endif /* SUPERBLOCK_DEF_H */
