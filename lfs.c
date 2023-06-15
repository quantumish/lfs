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

#define NOT_IMPLEMENTED(x) fprintf(stderr, "got call to %s (not implemented)\n", x)

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


static int lfs_readlink(const char *path, char *buf, size_t size) { NOT_IMPLEMENTED("readlink()"); }
static int lfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) { NOT_IMPLEMENTED("readdir()"); }
static int lfs_mknod(const char *path, mode_t mode, dev_t rdev) { NOT_IMPLEMENTED("mknod()"); }
static int lfs_mkdir(const char *path, mode_t mode) { NOT_IMPLEMENTED("mkdir()"); }
static int lfs_unlink(const char *path) { NOT_IMPLEMENTED("unlink()"); }
static int lfs_rmdir(const char *path) { NOT_IMPLEMENTED("rmdir()"); }
static int lfs_symlink(const char *from, const char *to) { NOT_IMPLEMENTED("symlink()"); }
static int lfs_rename(const char *from, const char *to) { NOT_IMPLEMENTED("rename()"); }
static int lfs_link(const char *from, const char *to) { NOT_IMPLEMENTED("link()"); }
static int lfs_chmod(const char *path, mode_t mode) { NOT_IMPLEMENTED("chmod()"); }
static int lfs_chown(const char *path, uid_t uid, gid_t gid) { NOT_IMPLEMENTED("chown()"); }
static int lfs_truncate(const char *path, off_t size) { NOT_IMPLEMENTED("truncate()"); }
#ifdef HAVE_UTIMENSAT
static int lfs_utimens(const char *path, const struct timespec ts[2], struct fuse_file_info *fi) { NOT_IMPLEMENTED("utimens()"); }
#endif
static int lfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) { NOT_IMPLEMENTED("read()"); }
static int lfs_statfs(const char *path, struct statvfs *stbuf) { NOT_IMPLEMENTED("statfs()"); }
static int lfs_release(const char *path, struct fuse_file_info *fi) { NOT_IMPLEMENTED("release()"); }
static int lfs_fsync(const char *path, int isdatasync, struct fuse_file_info *fi) { NOT_IMPLEMENTED("fsync()"); }
#ifdef HAVE_POSIX_FALLOCATE
static int lfs_fallocate(const char *path, int mode, off_t offset, off_t length, struct fuse_file_info *fi) { NOT_IMPLEMENTED("fallocate()"); }
#endif
static off_t lfs_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi) { NOT_IMPLEMENTED("lseek()"); }

#ifdef HAVE_SETXATTR
static int lfs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags) { NOT_IMPLEMENTED("setxattr()"); }
static int lfs_getxattr(const char *path, const char *name, char *value, size_t size) { NOT_IMPLEMENTED("getxattr()"); }
static int lfs_listxattr(const char *path, char *list, size_t size) { NOT_IMPLEMENTED("listxattr()"); }
static int lfs_removexattr(const char *path, const char *name) { NOT_IMPLEMENTED("removexattr()"); }
#endif

#ifdef HAVE_COPY_FILE_RANGE
static ssize_t lfs_copy_file_range(const char *path_in,
				   struct fuse_file_info *fi_in,
				   off_t offset_in, const char *path_out,
				   struct fuse_file_info *fi_out,
				   off_t offset_out, size_t len, int flags) { NOT_IMPLEMENTED("copy_file_range()"); }
#endif

static void* lfs_init(struct fuse_conn_info* conn) {
	printf("initializing\n");
	ctx_t* ctx = fuse_get_context()->private_data;
	char buf[BLOCK_SZ*2];
	read_blocks(buf, 2);
	memcpy(ctx->inode_map, buf+BLOCK_SZ, BLOCK_SZ);   
	return NULL;
}

static int lfs_access(const char *path, int mask) {
	return 0;
}

static int lfs_getattr(const char *path, struct stat *stbuf) {
	printf("getattr() on %s\n", path);
	bzero(stbuf, sizeof(struct stat));
	/* struct stat test; */
	/* int ret = lstat(path, stbuf); */
	lstat("/home/quantumish/projects/lfs/lfs_disk", stbuf);
	/* printf("mode is %d, ret is %d\n", stbuf->st_mode, ret); */
	/* stbuf->st_uid = test.st_uid; */
	/* stbuf->st_gid = test.st_gid; */
	if (strcmp(path, "/test") == 0) {
		stbuf->st_mode = 010777;
		return 0;
	} else if (strcmp(path, "/") == 0) {
		stbuf->st_mode = 040777;
		return 0;
	}	
	return -1;
}


static int lfs_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
	printf("whee\n");
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
	.init           = lfs_init,
	.getattr	= lfs_getattr,
	.access		= lfs_access,
	.readlink	= lfs_readlink,
	.readdir	= lfs_readdir,
	.mknod		= lfs_mknod,
	.mkdir		= lfs_mkdir,
	.symlink	= lfs_symlink,
	.unlink		= lfs_unlink,
	.rmdir		= lfs_rmdir,
	.rename		= lfs_rename,
	.link		= lfs_link,
	.chmod		= lfs_chmod,
	.chown		= lfs_chown,
	.truncate	= lfs_truncate,
#ifdef HAVE_UTIMENSAT
	.utimens	= lfs_utimens,
#endif
	.open		= lfs_open,
	.create 	= lfs_create,
	.read		= lfs_read,
	.write		= lfs_write,
	.statfs		= lfs_statfs,
	.release	= lfs_release,
	.fsync		= lfs_fsync,
#ifdef HAVE_POSIX_FALLOCATE
	.fallocate	= lfs_fallocate,
#endif
#ifdef HAVE_SETXATTR
	.setxattr	= lfs_setxattr,
	.getxattr	= lfs_getxattr,
	.listxattr	= lfs_listxattr,
	.removexattr	= lfs_removexattr,
#endif
#ifdef HAVE_COPY_FILE_RANGE
	.copy_file_range = lfs_copy_file_range,
#endif
	/* .lseek		= lfs_lseek, */
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
