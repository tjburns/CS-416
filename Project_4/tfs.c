#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>

#include "block.h"
#include "tfs.h"

char diskfile_path[PATH_MAX];

// Declare your in-memory data structures here
struct superblock super;
bitmap_t inode_bitmap;
bitmap_t datablock_bitmap;

struct inode inodes[1024];
// datablock declaration

/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

    // Step 1: Read inode bitmap from disk
    bio_read(1, &inode_bitmap); // TODO might have to open disk before this read
    // Step 2: Traverse inode bitmap to find an available slot
    int i;
    int inodeIndex = -1;
    for (i = 0; i < MAX_INUM; i++) {
        int currInode = get_bitmap(inode_bitmap, i);
        if (currInode == 0) {
            inodeIndex = i;
            break;
        }
    }
    // Step 3: Update inode bitmap and write to disk 
    // TODO set inode to inuse?
    return inodeIndex;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {

    // Step 1: Read data block bitmap from disk
    bio_read(2, &datablock_bitmap);
    // Step 2: Traverse data block bitmap to find an available slot
    int i;
    int datablockIndex = -1;
    for (i = 0; i < MAX_DNUM; i++) {
        int currDatablock = get_bitmap(datablock_bitmap, i);
        if (currDatablock == 0) {
            datablockIndex = i;
            break;
        }
    }
    // Step 3: Update data block bitmap and write to disk 
    // TODO set datablock to inuse?
    return datablockIndex;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {

  // Step 1: Get the inode's on-disk block number
    int i = ino / 16 + 3;
  // Step 2: Get offset of the inode in the inode on-disk block
    int offset = ino % 16 * BLOCK_SIZE;
  // Step 3: Read the block from disk and then copy into inode structure
    char * temp = malloc(INODE_SIZE);
    bio_read(i, temp);
    memcpy(inode, temp+offset, INODE_SIZE);
    free(temp);

    return 0;
}

int writei(uint16_t ino, struct inode *inode) {

    // Step 1: Get the block number where this inode resides on disk
    int ino = ino / 16 + 3;
    // Step 2: Get the offset in the block where this inode resides on disk
    int offset = ino % 16 * BLOCK_SIZE;
    // Step 3: Write inode to disk 
    char * temp = malloc(INODE_SIZE);
    memcpy(temp+offset, inode, INODE_SIZE);
    free(temp);

    return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)
    inode * inode = malloc(INODE_SIZE);
    readi(ino, inode);
  // Step 2: Get data block of current directory from inode
    char * block = malloc(BLOCK_SIZE);
    bio_read(inode.direct_ptr, block);
  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure
    struct dirent currDirent = malloc(sizeof(struct dirent));
    currDirent = (struct dirent) block;
    // finds a matching name within the directory
    int i = 0;
    for ( ; i < BLOCK_SIZE / sizeof(struct dirent); i++) {
        if (strcmp(currDirent.name, fname) == 0) {
            memcpy(dirent, currDirent, sizeof(struct dirent));
        }
        currDirent = currDirent + sizeof(struct dirent) * i;
    }

    return 0;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

    // Step 1: Read dir_inode's data block and check each directory entry of dir_inode
    char * dir_datablock = malloc(BLOCK_SIZE);
    bio_read(dir_inode.direct_ptr, dir_datablock);
    // Step 2: Check if fname (directory name) is already used in other entries
    int i = 0;
    struct dirent currDirent = malloc(sizeof(struct dirent));
    currDirent = (struct dirent) dir_datablock;
    for ( ; i < BLOCK_SIZE / sizeof(struct dirent); i++) {
        if (strcmp(currDirent.name, fname) == 0) {
            printf("Error: Name in use.\n");
            exit(1);
        }
        currDirent = currDirent + sizeof(struct dirent) * i;
    }
    // Step 3: Add directory entry in dir_inode's data block and write to disk
    struct dirent newDirent = malloc(sizeof(struct dirent));
    newDirent.name = fname;
    newDirent.ino = f_ino;
    newDirent.valid = 1;
    newDirent = dir_inode.direct_ptr;
    bio_write(ptr, newDirent);
    // return on successful read
    if (written == 1) {
        return 1;
    }
    // Allocate a new data block for this directory if it does not exist
    dir_inode.direct_ptr+1 = malloc(sizeof(BLOCK_SIZE));
    newDirent = dir_inode.direct_ptr+1;
    // Update directory inode
    dir_inode.ino = f_ino;
    dir_inode.valid = 1;
    dir_inode.size = sizeof(struct dirent);
    dir_inode.type = 1; // sets it as a directory type
    dir_inode.link = 1;
    dir_inode.direct_ptr = ptr+1;
    // Write directory entry
    bio_write(ptr, newDirent);

    return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

    // Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
    char * dir_datablock = malloc(sizeof(BLOCK_SIZE));
    bio_read(dir_inode.direct_ptr, dir_datablock);
    // Step 2: Check if fname exist
    struct dirent currDirent = malloc(sizeof(struct dirent));
    currDirent = (struct dirent) block;
    int i = 0;
    for ( ; i < BLOCK_SIZE / sizeof(struct dirent); i++) {
        // Step 3: If exist, then remove it from dir_inode's data block and write to disk
        if (strcmp(currDirent.name, fname) == 0) {
            currDirent = NULL;
            bio_write(dir_inode.direct_ptr, currDirent);
        }
        currDirent = currDirent + sizeof(struct dirent) * i;
    }
    
    return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
    
    // Step 1: Resolve the path name, walk through path, and finally, find its inode.
    // Note: You could either implement it in a iterative way or recursive way
    struct dirent newDir = malloc(sizeof(struct dirent));
    //TODO currpath setting based on "/"'s in path
    newDir.name = currPath;
    newDir.ino = ino;
    newDir.valid = 1;
    while(currPath != NULL) {
        //TODO currpath setting based on "/"'s in path
        newDir = dir_find(ino, currPath, sizeof(currPath), newDir);
    }
    memcpy(inodes[newDir.ino], inode);

    return 0;
}

/* 
 * Make file system
 */
int tfs_mkfs() {

    // Call dev_init() to initialize (Create) Diskfile
    dev_init(diskfile_path);

    // write superblock information
    //struct superblock super; --- superblock here shouldn't have to be malloc()-ed
    super.magic_num = MAGIC_NUM;
    super.max_inum = MAX_INUM;
    super.max_dnum = MAX_INUM;
    super.i_bitmap_blk = 1; // TODO find addresses somehow
    super.d_bitmap_blk = 2;
    super.i_start_blk = 3; 
    super.d_start_blk = 68; //should be (3+64)+1
    bio_write(0, &super)

    // initialize inode bitmap
    inode_bitmap = malloc(MAX_INUM/8);
    bio_write(super.i_bitmap_blk, inode_bitmap);
    /* should already be initialized to 0 based on properties of malloc
    int i;
    for (i = 0; i < 128; i++) {
        set_bitmap(inode_bitmap, 0);
    } */

    // initialize data block bitmap
    datablock_bitmap = malloc(MAX_DNUM/8);
    bio_write(super.d_bitmap_blk, datablock_bitmap);

    // update bitmap information for root directory
    //TODO figure out what needs to be done here
    // update inode for root directory

    return 0;
}


/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {

    // Step 1a: If disk file is not found, call mkfs
    if (dev_open(diskfile_path) != 0) {
        tfs_mkfs();
    }
    else {
        bio_read(0, &super);
        if (temp.magic_num != MAGIC_NUM) {
            tfs_mkfs();
        }
        else {
            // Step 1b: If disk file is found, just initialize in-memory data structures
              // and read superblock from disk
            bio_read(0, &super); // redundant
            bio_read(1, inode_bitmap);
            bio_read(2, datablock_bitmap);
            bio_read(3, inodes);
            // read datablock?
        }
    }

    return NULL;
}

static void tfs_destroy(void *userdata) {

    // Step 1: De-allocate in-memory data structures
    //superblock is not malloc()-ed as of now
    free(inode_bitmap); // TODO check if this segfaults
    free(datablock_bitmap);
    // Step 2: Close diskfile
    dev_close();
}

static int tfs_getattr(const char *path, struct stat *stbuf) {

    // Step 1: call get_node_by_path() to get inode from path
    int ino = get_node_by_path(path, 0, NULL);
    // Step 2: fill attribute of file into stbuf from inode
    stbuf->st_mode   = S_IFDIR | 0755;
    stbuf->st_nlink  = 2;
    time(&stbuf->st_mtime);

    return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {

    // Step 1: Call get_node_by_path() to get inode from path
    struct inode node = malloc(sizeof(struct inode));
    get_node_by_path(path, 0, node);
    // Step 2: If not find, return -1
    if (node == NULL) {
        return -1;
    }

    return 0;
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

    // Step 1: Call get_node_by_path() to get inode from path
    struct inode node = malloc(sizeof(struct inode));
    get_node_by_path(path, 0, node);
    // Step 2: Read directory entries from its data blocks, and copy them to filler
    bio_read(node.direct_ptr, buffer);
    struct dirent * dirents = (struct dirent) buffer;
    memcpy(filler, dirents, sizeof(struct dirent));

    return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {

    // Step 1: Use dirname() and basename() to separate parent directory path and target directory name
    char * dname = dirname(path);
    char * bname = basename(path);
    // Step 2: Call get_node_by_path() to get inode of parent directory
    struct inode node = malloc(sizeof(struct inode));
    get_node_by_path(path, 0, node);
    // Step 3: Call get_avail_ino() to get an available inode number
    int ino = get_avail_ino();
    // Step 4: Call dir_add() to add directory entry of target directory to parent directory
    dir_add(node, ino, dname, sizeof(dname));
    // Step 5: Update inode for target directory
    node.ino = ino;
    node.valid = 1;
    node.size = sizeof(struct dirent);
    node.type = 1;
    node.link = 1;
    node.direct_ptr = NULL;
    // Step 6: Call writei() to write inode to disk
    writei(node.ino, node);

    return 0;
}

static int tfs_rmdir(const char *path) {

    // Step 1: Use dirname() and basename() to separate parent directory path and target directory name
    char * dname = dirname(path);
    char * bname = basename(path);
    // Step 2: Call get_node_by_path() to get inode of target directory
    struct inode node = malloc(sizeof(struct inode));
    get_node_by_path(path, 0, node);
    // Step 3: Clear data block bitmap of target directory
    int i = 0;
    for ( ; i < sizeof(struct dirent)) {
        unset_bitmap(datablock_bitmap, i*node.direct_ptr);
    }
    // Step 4: Clear inode bitmap and its data block
    for ( ; i < INODE_SIZE; i++) {
        unset_bitmap(inode_bitmap, i*node.ino);
    }
    // Step 5: Call get_node_by_path() to get inode of parent directory
    struct inode node2 = malloc(sizeof(struct inode));
    get_node_by_path(bname, 0, node2);
    // Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory
    dir_remove(node2, bname, sizeof(bname));

    return 0;
}

static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {
    // For this project, you don't need to fill this function
    // But DO NOT DELETE IT!
    return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

    // Step 1: Use dirname() and basename() to separate parent directory path and target file name
    char * dname = dirname(path);
    char * bname = basename(path);
    // Step 2: Call get_node_by_path() to get inode of parent directory
    struct inode node = malloc(sizeof(struct inode));
    get_node_by_path(path, 0, node);
    // Step 3: Call get_avail_ino() to get an available inode number
    int ino = get_avail_ino();
    // Step 4: Call dir_add() to add directory entry of target file to parent directory
    dir_add(node, ino, dname, sizeof(dname));
    // Step 5: Update inode for target file
    node.ino = ino;
    node.valid = 1; 
    node.size = sizeof(struct dirent);
    node.type = 1; // 2 designates the entry is a file
    node.link = 1;
    node.direct_ptr = NULL;
    // Step 6: Call writei() to write inode to disk
    writei(ino, node);

    return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {

    // Step 1: Call get_node_by_path() to get inode from path
    struct inode node = malloc(sizeof(struct inode));
    get_node_by_path(path, 0, node);
    // Step 2: If not find, return -1
    if (node == NULL) {
        return -1;
    }

    return 0;
}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

    // Step 1: You could call get_node_by_path() to get inode from path
    struct inode node = malloc(sizeof(struct inode));
    get_node_by_path(path, 0, node);
    // Step 2: Based on size and offset, read its data blocks from disk
    // Step 3: copy the correct amount of data from offset to buffer
    pwrite(diskfile, buffer, size, node.direct_ptr*BLOCK_SIZE+offset);
    // Note: this function should return the amount of bytes you copied to buffer
    return size;

    return 0;
}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
    // Step 1: You could call get_node_by_path() to get inode from path
    struct inode node = malloc(sizeof(struct inode));
    get_node_by_path(path, 0, node);
    // Step 2: Based on size and offset, read its data blocks from disk
    // Step 3: Write the correct amount of data from offset to disk
    // instead of writing from disk to buffer -> write buffer to disk
    pwrite(buffer, diskfile, size, node.direct_ptr*BLOCK_SIZE+offset);
    // Step 4: Update the inode info and write it to disk
    // Note: this function should return the amount of bytes you write to disk
    return size;
}

static int tfs_unlink(const char *path) {

    // Step 1: Use dirname() and basename() to separate parent directory path and target file name
    char * dname = dirname(path);
    char * bname = basename(path);
    // Step 2: Call get_node_by_path() to get inode of target file
    struct inode node = malloc(sizeof(struct inode));
    get_node_by_path(path, 0, node);
    // Step 3: Clear data block bitmap of target file
    int i = 0;
    for ( ; i < sizeof(struct dirent)) {
        unset_bitmap(datablock_bitmap, i*node.direct_ptr);
    }
    // Step 4: Clear inode bitmap and its data block
    for ( ; i < INODE_SIZE; i++) {
        unset_bitmap(inode_bitmap, i*node.ino);
    }
    // Step 5: Call get_node_by_path() to get inode of parent directory
    struct inode node2 = malloc(sizeof(struct inode));
    get_node_by_path(bname, 0, node2);
    // Step 6: Call dir_remove() to remove directory entry of target file in its parent directory
    dir_remove(node2, bname, sizeof(bname));

    return 0;
}

static int tfs_truncate(const char *path, off_t size) {
    // For this project, you don't need to fill this function
    // But DO NOT DELETE IT!
    return 0;
}

static int tfs_release(const char *path, struct fuse_file_info *fi) {
    // For this project, you don't need to fill this function
    // But DO NOT DELETE IT!
    return 0;
}

static int tfs_flush(const char * path, struct fuse_file_info * fi) {
    // For this project, you don't need to fill this function
    // But DO NOT DELETE IT!
    return 0;
}

static int tfs_utimens(const char *path, const struct timespec tv[2]) {
    // For this project, you don't need to fill this function
    // But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations tfs_ope = {
    .init		= tfs_init,
    .destroy	= tfs_destroy,

    .getattr	= tfs_getattr,
    .readdir	= tfs_readdir,
    .opendir	= tfs_opendir,
    .releasedir	= tfs_releasedir,
    .mkdir		= tfs_mkdir,
    .rmdir		= tfs_rmdir,

    .create		= tfs_create,
    .open		= tfs_open,
    .read 		= tfs_read,
    .write		= tfs_write,
    .unlink		= tfs_unlink,

    .truncate   = tfs_truncate,
    .flush      = tfs_flush,
    .utimens    = tfs_utimens,
    .release	= tfs_release
};


int main(int argc, char *argv[]) {
    int fuse_stat;

    getcwd(diskfile_path, PATH_MAX);
    strcat(diskfile_path, "/DISKFILE");

    fuse_stat = fuse_main(argc, argv, &tfs_ope, NULL);

    return fuse_stat;
}

