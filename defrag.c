#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "disk_image.h"
#include "inode_scan.h"
#include "layout_plan.h"
#include "block_rewrite.h"
#include "file_records.h"
#include "freelist.h"
#include "verify.h"
#include "util.h"
#include "superblock_def.h"
#include <sys/stat.h>

static int verbose = 0;

static int load_file(const char *path, unsigned char **buf, size_t *size) {
	struct stat st;
	if (stat(path, &st) != 0) return -1;
	FILE *f = fopen(path, "rb");
	if (!f) return -1;
	unsigned char *b = (unsigned char *)malloc((size_t)st.st_size);
	if (!b) { fclose(f); return -1; }
	size_t rd = fread(b, 1, (size_t)st.st_size, f);
	fclose(f);
	if (rd != (size_t)st.st_size) { free(b); return -1; }
	*buf = b; *size = rd; return 0;
}

static int compare_images(const char *pathA, const char *pathB) {
	unsigned char *a = NULL, *b = NULL; size_t sa = 0, sb = 0;
	if (load_file(pathA, &a, &sa) != 0) return -1;
	if (load_file(pathB, &b, &sb) != 0) { free(a); return -1; }
	int eq = (sa == sb) && (memcmp(a, b, sa) == 0);
	free(a); free(b);
	return eq ? 0 : 1;
}

int main(int argc, char *argv[]) {
	const char *input_path = NULL;
	const char *verify_path = NULL;
	/* Args: defrag [-q] <input> [--verify <expected>] */
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-q") == 0) { verbose = 0; continue; }
		if (strcmp(argv[i], "--verify") == 0 && i + 1 < argc) { verify_path = argv[++i]; continue; }
		if (!input_path) { input_path = argv[i]; continue; }
	}
	if (!input_path) {
		fprintf(stderr, "Usage: %s [-q] <input_disk_image> [--verify <expected_image>]\n", argv[0]);
		return 1;
	}

	unsigned char *in_buf = NULL;
	size_t in_size = 0;
	if (load_disk_image(input_path, &in_buf, &in_size) != 0) {
		fatal("Failed to load disk image");
	}

	struct superblock sb;
	if (parse_superblock(in_buf, &sb) != 0) {
		fatal("Failed to parse superblock");
	}

	if (verbose) {
		printf("Superblock:\n");
		printf("  blocksize    = %d\n", sb.blocksize);
		printf("  inode_offset = %d blocks\n", sb.inode_offset);
		printf("  data_offset  = %d blocks\n", sb.data_offset);
		printf("  swap_offset  = %d blocks\n", sb.swap_offset);
		printf("  free_inode   = %d\n", sb.free_inode);
		printf("  free_block   = %d\n", sb.free_block);
		printf("Image size: %zu bytes\n", in_size);
	}

	/* Scan inodes and print summary */
	int inode_used_count = 0;
	/* First pass to get count */
	if (scan_inodes(in_buf, &sb, NULL, &inode_used_count) != 0) {
		fatal("Failed to scan inodes");
	}
	if (verbose) printf("Used inodes: %d\n", inode_used_count);

	/* Build FileRecords from used inodes (direct blocks for now) */
	if (inode_used_count > 0) {
		/* Re-scan to get views again */
		InodeView *views = (InodeView *)malloc(sizeof(InodeView) * (size_t)inode_used_count);
		int tmp = 0;
		if (!views || scan_inodes(in_buf, &sb, views, &tmp) != 0 || tmp != inode_used_count) {
			free(views);
			fatal("Failed to prepare InodeView for records");
		}
		FileRecord *records = NULL; int rec_count = 0;
		if (build_file_records(in_buf, &sb, views, inode_used_count, &records, &rec_count) != 0) {
			free(views);
			fatal("Failed to build file records");
		}
		if (verbose) {
			for (int i = 0; i < rec_count; ++i) {
				printf("  file inode=%d blocks=%d direct=%d\n", records[i].inode_index,
					   records[i].data_block_count, records[i].direct_count);
			}
		}

		/* Plan contiguous layout */
		FilePlacement *placements = (FilePlacement *)malloc(sizeof(FilePlacement) * (size_t)rec_count);
		int place_count = 0; int next_free = 0;
		if (!placements || plan_layout(&sb, records, rec_count, placements, &place_count, &next_free) != 0) {
			free(placements);
			free_file_records(records, rec_count);
			free(views);
			fatal("Layout planning failed");
		}
		if (verbose) {
			printf("Layout plan:\n");
			for (int i = 0; i < place_count; ++i) {
				printf("  inode=%d start=%d ptr_blocks=%d data_blocks=%d\n",
					   placements[i].inode_index,
					   placements[i].start_block,
					   placements[i].pointer_block_count,
					   placements[i].data_block_count);
			}
			printf("Next free block index: %d\n", next_free);
		}

		/* Build block mapping (direct + single-indirect) */
		/* Prepare output buffer same size as input */
		unsigned char *out_buf = (unsigned char *)malloc(in_size);
		if (!out_buf) {
			free(placements);
			free_file_records(records, rec_count);
			free(views);
			fatal("malloc failed for output buffer");
		}
		/* Copy boot block and superblock as-is */
		memcpy(out_buf, in_buf, 512 + 512);
		/* Copy entire inode region from input before rewriting selected inodes */
		size_t inode_region_abs = (512 + 512) + (size_t)sb.inode_offset * (size_t)sb.blocksize;
		size_t inode_region_size = (size_t)(sb.data_offset - sb.inode_offset) * (size_t)sb.blocksize;
		memcpy(out_buf + inode_region_abs, in_buf + inode_region_abs, inode_region_size);

		RewriteContext ctx = {
			.sb = &sb,
			.in_buf = in_buf,
			.out_buf = out_buf,
			.records = records,
			.placements = placements,
			.count = rec_count,
			.map = NULL,
			.map_size = 0
		};
		if (build_block_mapping(&ctx) != 0) {
			free(placements);
			free_file_records(records, rec_count);
			free(views);
			fatal("Failed to build block mapping");
		}
		if (verbose) printf("Mappings built: %d entries\n", ctx.map_size);
		/* Rewrite inodes (into output buffer) */
		if (rewrite_inodes(&ctx) != 0) {
			free(ctx.map);
			free(out_buf);
			free(placements);
			free_file_records(records, rec_count);
			free(views);
			fatal("rewrite_inodes failed");
		}
		/* Rewrite pointer blocks */
		if (rewrite_pointer_blocks(&ctx) != 0) {
			free(ctx.map);
			free(out_buf);
			free(placements);
			free_file_records(records, rec_count);
			free(views);
			fatal("rewrite_pointer_blocks failed");
		}
		/* Rewrite data blocks */
		if (rewrite_data_blocks(&ctx) != 0) {
			free(ctx.map);
			free(out_buf);
			free(placements);
			free_file_records(records, rec_count);
			free(views);
			fatal("rewrite_data_blocks failed");
		}

		/* Rebuild free block list and update superblock free_block */
		int total_data_blocks = sb.swap_offset - sb.data_offset; /* blocks in data region */
		if (rebuild_free_block_list(out_buf, &sb, next_free, total_data_blocks) != 0) {
			free(ctx.map);
			free(out_buf);
			free(placements);
			free_file_records(records, rec_count);
			free(views);
			fatal("rebuild_free_block_list failed");
		}

		/* Do not alter inode free list; preserve original inode metadata except pointers */
		/* Update free_block in superblock of out_buf */
		size_t sb_off = 512; /* superblock starts after boot block */
		int head = next_free;
		out_buf[sb_off + 20] = (unsigned char)(head & 0xFF);
		out_buf[sb_off + 21] = (unsigned char)((head >> 8) & 0xFF);
		out_buf[sb_off + 22] = (unsigned char)((head >> 16) & 0xFF);
		out_buf[sb_off + 23] = (unsigned char)((head >> 24) & 0xFF);

		/* Copy swap region unchanged */
		size_t swap_abs = (512 + 512) + (size_t)sb.swap_offset * (size_t)sb.blocksize;
		size_t total_size = in_size;
		memcpy(out_buf + swap_abs, in_buf + swap_abs, total_size - swap_abs);

		/* Write output image */
		if (write_disk_image("disk_defrag", out_buf, in_size) != 0) {
			free(ctx.map);
			free(out_buf);
			free(placements);
			free_file_records(records, rec_count);
			free(views);
			fatal("Failed to write disk_defrag");
		}
		if (verbose) printf("Wrote disk_defrag\n");

		if (verify_path) {
			int rc = compare_images("disk_defrag", verify_path);
			if (rc == 0) {
				printf("Verify: Images are identical\n");
			} else if (rc > 0) {
				printf("Verify: Images differ\n");
			} else {
				printf("Verify: Comparison failed\n");
			}
		}

		free(ctx.map);
		free(out_buf);
		free(placements);
		free_file_records(records, rec_count);
		free(views);
	}

	free(in_buf);
	return 0;
}
