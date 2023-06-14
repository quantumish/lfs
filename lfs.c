#define FUSE_USE_VERSION 31
#define _GNU_SOURCE
#define _XOPEN_SOURCE 700
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

typedef unsigned int diskptr_t;

#define DEFAULT_DISK_SZ 1024

#define BLOCK_SZ 512
#define MAX_FILES 256

struct inode {
    size_t size;
    size_t blocks;
    diskptr_t block[16];
};

#define MAX_OPEN_FILES 4
typedef struct ctx {
	int disk;
	diskptr_t inode_map[MAX_FILES];	
	struct inode cur_files[MAX_OPEN_FILES];
} ctx_t;

// Writes a `buf` of `len` bytes padded to the nearest number of blocks on disk.
void write_blocks(const char* buf, size_t len) {
	ctx_t* ctx = fuse_get_context()->private_data;
    for (size_t i = 0; i < (len/BLOCK_SZ); i++) {
        write(ctx->disk, buf+(i*BLOCK_SZ), BLOCK_SZ);
    }
	if (len % BLOCK_SZ != 0) {
		char end_buf[BLOCK_SZ] = {0};
		memcpy(end_buf, buf+((len/BLOCK_SZ)*BLOCK_SZ), len % BLOCK_SZ);
		write(ctx->disk, end_buf, BLOCK_SZ); 
	}
}

// Reads `num` blocks at the current position in disk to user-specified `buf`
void read_blocks(char* buf, size_t num) {
	ctx_t* ctx = fuse_get_context()->private_data;
	for (size_t i = 0; i < num; i++) {
		read(ctx->disk, buf+(i*BLOCK_SZ), BLOCK_SZ);
	}
}

// Reads block at `loc` into user-specified `buf`. No side-effects, seeks back.
void read_block(char* buf, size_t loc) {
	ctx_t* ctx = fuse_get_context()->private_data;
	off_t cur_loc = lseek(ctx->disk, 0, SEEK_CUR);
	lseek(ctx->disk, loc*BLOCK_SZ, SEEK_SET);
	read(ctx->disk, buf, BLOCK_SZ);
	lseek(ctx->disk, cur_loc, SEEK_SET);
}

diskptr_t current_block() {
	ctx_t* ctx = fuse_get_context()->private_data;
    return lseek(ctx->disk, 0, SEEK_CUR)/BLOCK_SZ;
}

int get_fd() {
	ctx_t* ctx = fuse_get_context()->private_data;
    char zero[sizeof(struct inode)] = {0}; // gross
    for (size_t i = 0; i < MAX_OPEN_FILES; i++) {
        if (memcmp(ctx->cur_files+i, zero, sizeof(struct inode)) == 0) return i;
    }
    return -1;
}

static void* lfs_init(struct fuse_conn_info* conn) {
	ctx_t* ctx = fuse_get_context()->private_data;
	char buf[BLOCK_SZ*2];
	read_blocks(buf, 2);
	memcpy(ctx->inode_map, buf+BLOCK_SZ, BLOCK_SZ);   
	return NULL;
}

static int lfs_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
	fi->fh = get_fd();
	return 0;
}

static int lfs_open(const char* path, struct fuse_file_info* fi) {
	fi->fh = get_fd();
	return 0;
}

static int lfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {	
	ctx_t* ctx = fuse_get_context()->private_data;
	int fd = fi->fh;
	write_blocks(buf, size);
	struct inode inode = ctx->cur_files[fd];
	size_t blocks_written = (size+(BLOCK_SZ-1))/BLOCK_SZ;    
    inode.size = size;
    diskptr_t data_start = current_block()-blocks_written;
    for (size_t i = 0; i < inode.blocks; i++) {
        inode.block[i] = data_start+i;
    }
    inode.blocks = blocks_written;
	write_blocks((char*)&inode, sizeof(inode));	
	return size;
}

static const struct fuse_operations lfs_oper = {
	.init = lfs_init,
	.create = lfs_create,
	.open = lfs_open,
	.write = lfs_write,
};

int main(int argc, char** argv) {
	if (argc < 3) {
		fprintf(stderr, "lfs: usage is either 'lfs [disk] [mountpoint]' or 'lfs mkfs [disk]'\n");
		return 1;
	}

	ctx_t* ctx = calloc(sizeof(ctx_t), 1);	
	if (strcmp(argv[1], "mkfs") == 0) {
		ctx->disk = open(argv[2], O_WRONLY | O_CREAT, 0644);
		char buf[BLOCK_SZ] = {0};
		memcpy(buf, "LFS", 4);
		write(ctx->disk, buf, BLOCK_SZ);		
		for (size_t i = 0; i < DEFAULT_DISK_SZ-1; i++) {
			bzero(buf, 4);
		    write(ctx->disk, buf, BLOCK_SZ);
		}
		close(ctx->disk);
		return 0;
	}

	umask(0);
	ctx->disk = open(argv[1], O_RDWR);	
	return fuse_main(argc-1, argv+1, &lfs_oper, ctx);
}
