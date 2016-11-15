#ifndef _INCLUDE_SFS_API_H_
#define _INCLUDE_SFS_API_H_

#include <stdint.h>

#define MAXFILENAME 20
#define NUM_BLOCKS 100000
#define NUM_INODES 50
#define NUM_INDIRECT NUM_BLOCKS / sizeof(unsigned int)


/*
 * magic
 * block_size           Block size of the file system
 * fs_size              Size of the file system
 * inode_table_len      Length of the inode table
 * root_dir_inode       pointer to the inode of the root
 */
typedef struct{
    uint64_t magic;
    uint64_t block_size;
    uint64_t fs_size;
    uint64_t inode_table_len;
    uint64_t root_dir_inode;
} superblock_t;



/*
 * used             if this inode is being used
 * mode
 * link_cnt
 * uid
 * gid
 * size             size of the file
 * data_ptrs        direct data pointer
 * indirect_ptrs    pointer to the structure containing the indirect pointer
 */
typedef struct {
    unsigned int used;
    unsigned int mode;
    unsigned int link_cnt;
    unsigned int uid;
    unsigned int gid;
    unsigned int size;
    unsigned int data_ptrs[12];
    unsigned int indirect_ptrs;
} inode_t;



/*
 * data_ptr     indirect pointer
 */
typedef struct {
    unsigned  int data_ptr[ NUM_INDIRECT ];
} indirect_t;



/*
 * used     if this entry is being used
 * inode    which inode this entry describes
 * name     name of the file associated with the inode
 */
typedef struct {
    uint64_t used;
    uint64_t inode;
    char* name;
} entry_t;



/*
 * inode    which inode this entry describes
 * rwptr    where in the file to start
 */
typedef struct {
    uint64_t used;
    uint64_t inode;
    uint64_t rwptr;
} file_descriptor;

void mksfs(int fresh);
int sfs_getnextfilename(char *fname);
int sfs_getfilesize(const char* path);
int sfs_fopen(char *name);
int sfs_fclose(int fileID);
int sfs_fread(int fileID, char *buf, int length);
int sfs_fwrite(int fileID, const char *buf, int length);
int sfs_fseek(int fileID, int loc);
int sfs_remove(char *file);

#endif //_INCLUDE_SFS_API_H_
