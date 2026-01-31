CC=gcc
CFLAGS=-std=c11 -O0 -g -Wall -Wextra -pedantic

SRC=defrag.c \
    disk_image.c \
    inode_scan.c \
    layout_plan.c \
    file_records.c \
    block_rewrite.c \
    freelist.c \
    verify.c \
    util.c

OBJ=$(SRC:.c=.o)

all: defrag

defrag: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

clean:
	rm -f $(OBJ) defrag

.PHONY: all clean
