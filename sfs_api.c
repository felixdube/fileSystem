#include "sfs_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "disk_emu.h"
#include "bitmap.h"

#define JITS_DISK "sfs_disk.disk"
#define BLOCK_SZ 1024

#define NUM_INODE_BLOCKS (sizeof(inode_t) * NUM_INODES / BLOCK_SZ + 1) 
#define NUM_DIR_BLOCKS (sizeof(entry_t) * NUM_INODES / BLOCK_SZ + 1) 
#define MAX_RWPTR ((12*BLOCK_SZ) + (BLOCK_SZ/4)*BLOCK_SZ)

superblock_t sb;
inode_t inode_table[NUM_INODES];
entry_t directory_table[NUM_INODES-1];
int directory_table_index = -1;
file_descriptor fdt[NUM_INODES];



/**
 * @brief Append a character to a string
 * @param char* String to which we want to append
 * @param char Character to be appended
 * @param int Length of the string
 * @retval None
 */
char* appendCharToCharArray(char* array, char a, int len)
{
    char* ret = malloc((len+2)*sizeof(char));

    int i;
    for(i = 0; i < len; i++) {
        ret[i] = array[i];
    }
    ret[len] = a;
    ret[len+1] = '\0';

    return ret;
}



/**
 * @brief Initialize the super block
 * @retval None
 */
void init_superblock() {
    sb.magic = 0xACBD0005;
    sb.block_size = BLOCK_SZ;
    sb.fs_size = NUM_BLOCKS * BLOCK_SZ;
    sb.inode_table_len = NUM_INODE_BLOCKS;
    sb.root_dir_inode = 0;
}



/**
 * @brief Initialize the root directory
 * @retval None
 */
void init_root_directory() {

    // inode
    inode_t rd;
    rd.used = 1;
    rd.mode = 777;
    rd.link_cnt = 1;
    rd.uid = 0;
    rd.gid = 0;
    rd.size = 0;

    inode_table[0] = rd;

    // directory
    int i;
    for(i = 0; i < sizeof(directory_table)/ sizeof(directory_table[0]); i++){
        directory_table[i].used = 0;
    }
}



/**
 * @brief Initialize the inode table
 * @retval None
 */
void init_inode_table() {
    int i;
    for(i = 1; i < sizeof(inode_table)/ sizeof(inode_table[0]); i++){
        inode_table[i].used = 0;
    }
}



/**
 * @brief Initialize the file descriptor table
 * @retval None
 */
void init_file_descriptor() {
    int i;
    for(i = 0; i < sizeof(fdt)/ sizeof(fdt); i++){
        fdt[i].used = 0;
    }
}



/**
 * @brief Make or open a file system
 * @param int Boolean deciding on making a new file or openning an existing one
 * @retval None
 */
void mksfs(int fresh) {

	//Implement mksfs here
    if (fresh) {
        printf("SFS > Making new file system\n");

        // create super block
        init_superblock();

        init_inode_table();

        //create rot directory
        init_root_directory();

        init_fresh_disk(JITS_DISK, BLOCK_SZ, NUM_BLOCKS);

        // write free block list
        //super block bitmap
        force_set_index(0);

        // inode table bitmap
        int i;
        for(i = 1; i <= sb.inode_table_len; i++){
            force_set_index(i);
        }

        // force the bit for the directory table
        // and assign the data pointer in the directory i node
        int j = 0;
        for(i = (int) sb.inode_table_len +1; i <= sb.inode_table_len + NUM_DIR_BLOCKS +1; i++){
            force_set_index(i);
            inode_table[0].data_ptrs[j] = (unsigned) i;
            j++;
        }

        // init the other unassigned data pointer
        while(j< 12){
            inode_table[0].data_ptrs[j] = -1;
            j++;
        }
        inode_table[0].indirect_ptrs = -1;

        // bitmap bitmap
        //uint8_t free_bit_map[SIZE] = { [0 ... SIZE-1] = UINT8_MAX };

        uint8_t* free_bit_map = get_bitmap();
        force_set_index(NUM_BLOCKS-1);
        write_blocks(NUM_BLOCKS - 1, 1, (void*) free_bit_map);
        
        /* write super block
         * write to first block, and only take up one block of space
         * pass in sb as a pointer
         */
        write_blocks(0, 1, (void*) &sb);

        // write inode table
        write_blocks(1, (int) sb.inode_table_len, (void*) inode_table);

        //write directory table
        write_blocks((int) sb.inode_table_len + 1, NUM_DIR_BLOCKS, (void*) directory_table);

    } else {
        printf("SFS > Reopening file system\n");
        // open super block
        read_blocks(0, 1, &sb);
        printf("SFS > Block Size is: %i \n", (int) sb.block_size);

        // open inode table
        read_blocks(1, (int) sb.inode_table_len, (void*) inode_table);

        // open directory_table
        read_blocks((int) sb.inode_table_len + 1, NUM_DIR_BLOCKS, (void*) directory_table);

        // open free block list
        uint8_t* free_bit_map = get_bitmap();
        read_blocks(NUM_BLOCKS - 1, 1, (void*) free_bit_map);
    }
	return;
}



/**
 * @brief Get the name of the next file on the directory table
 * @param char* Name of the file
 * @retval int The amount of file left
 */
int sfs_getnextfilename(char *fname) {

    directory_table_index++;
    int count = 1;

    //check if you are at the end of the table
    if(directory_table_index == sizeof(directory_table)/ sizeof(directory_table[0])) {
        directory_table_index = -1;
        return 0;
    }

    // find the next directory that is used
    while(directory_table[directory_table_index].used == 0) {
        directory_table_index++;
        // check if you are at the end of the table
        if(directory_table_index == sizeof(directory_table)/ sizeof(directory_table[0])){
            directory_table_index = 0;
            return 0;
        }

        // check if you have gone through all the table
        if(count == sizeof(directory_table)/ sizeof(directory_table[0])) {
            printf("SFS > There is no file in the directory.\n");
            return 0;
        }
        count++;
    }

    int i;
    for (i = 0; i< strlen(directory_table[directory_table_index].name); i++){
        fname[i] = (directory_table[directory_table_index].name)[i];
    }
    fname[i] = '\0';


	// return how many entry there is left in the directory
    return sizeof(directory_table)/ sizeof(directory_table[0]) - directory_table_index -1;
}



/**
 * @brief Get the size of a file
 * @param cont char* Name of the file
 * @retval int Size of the file
 */
int sfs_getfilesize(const char* path) {

    int i;
	for(i = 0; i < sizeof(directory_table)/ sizeof(directory_table[0]); i++) {
        if(directory_table[i].used == 1) {
            if (strcmp(directory_table[i].name, path) == 0) {
                return inode_table[directory_table[i].inode].size;
            }
        }
    }

    printf("SFS > File %s not found when getting size!\n", path);
	return 0;
}



/**
 * @brief Open a file, if it does not exit a new file is created
 * @param char Name of the file to open
 * @retval int The file ID
 */
int sfs_fopen(char *name) {


    if(strlen(name) > MAXFILENAME || strlen(name) == 0){
        return -1;
    }

    // try to find the file
    uint64_t inode_table_index = -1;
    int i;
    for(i = 0; i < sizeof(directory_table)/ sizeof(directory_table[0]); i++) {
        if(directory_table[i].used == 1) {
            if (strcmp(directory_table[i].name, name) == 0) {
                inode_table_index = (int) directory_table[i].inode;
            }
        }
    }

	/* FILE DOES NOT EXIST */
    if(inode_table_index == -1) {

        // find the first empty spot in the inode_table
        uint64_t new_inode_table_index = 0;
        while(inode_table[ new_inode_table_index ].used == 1 && new_inode_table_index < sizeof(inode_table)/ sizeof(inode_table[0])) {
            new_inode_table_index++;

            // if there is no more space in the table
            if(new_inode_table_index == sizeof(inode_table)/ sizeof(inode_table[0])){
                printf("SFS > There is no more space in the inode table!\n");
                return -1;
            }

        }

        // find the first empty spot in the directory_table
        int  new_entry_index = 0;
        while(directory_table[ new_entry_index ].used == 1 && new_entry_index < sizeof(directory_table)/ sizeof(directory_table[0])) {
            new_entry_index++;

            // if there is no space in the table
            if(new_entry_index == sizeof(directory_table)/ sizeof(directory_table[0])){
                printf("SFS > No more space in the directory table! \n");
                return -1;
            }
        }

        // initialize the inode
        inode_t new_inode;
        new_inode.used = 1;
        new_inode.mode = 777;
        new_inode.link_cnt = 1;
        new_inode.uid = 0;
        new_inode.gid = 0;
        new_inode.size = 0;
        int j;
        for(j = 0; j< 12; j++) {
            new_inode.data_ptrs[j] = -1;
        }
        new_inode.indirect_ptrs = -1;

        // update inode_table in memory
        inode_table[ new_inode_table_index ] = new_inode;

        // initialize the new entry
        entry_t new_entry;
        new_entry.used = 1;
        new_entry.inode = new_inode_table_index;
        new_entry.name = malloc(sizeof(char)*(strlen(name)+1));
        int w;
        for (w = 0; w< strlen(name) +1 ; w++){
            new_entry.name[w] = name[w];
        }
        new_entry.name[w] = '\0';

        directory_table[new_entry_index] = new_entry;

        // update inode table on disk
        write_blocks(1, (int) sb.inode_table_len, (void*) inode_table);

        // update directory table on disk
        write_blocks((int) sb.inode_table_len + 1, NUM_DIR_BLOCKS, (void*) directory_table);

        inode_table_index = new_inode_table_index;
    }

    // check is the file is already open
    int open_file_index;
    for(open_file_index = 0; open_file_index < sizeof(fdt)/ sizeof(fdt[0]); open_file_index++){
        if(fdt[open_file_index].inode == inode_table_index){
            return open_file_index;
        }
    }


    // find a spot on the file descriptor table
    int  new_fdt_index = 0;
    while(fdt[ new_fdt_index ].used == 1) {
        new_fdt_index++;
        if(new_fdt_index == sizeof(fdt)/ sizeof(fdt[0])) {
            printf("SFS > There is no more space in the file descriptor table!\n");
            return -1;
        }
    }

    fdt[new_fdt_index].used = 1;
    fdt[new_fdt_index].inode = inode_table_index;
    fdt[new_fdt_index].rwptr = 0;


	return new_fdt_index;
}



/**
 * @brief Close a file that was open
 * @param int File ID of an open file
 * @retval int Return zero on success
 */
int sfs_fclose(int fileID){
	// check if the there is a file open
    if(fdt[fileID].used == 0) {
        printf("SFS > The file %i was not used! \n", fileID);
        return -1;
    }

    fdt[fileID].used = 0;
    fdt[fileID].inode = -1;

	return 0;
}



/**
 * @brief Read some data to the a file
 * @param int File ID of an open file
 * @param const char Buffer for the data read
 * @oaram int Length of the data
 * @retval int The number of bytes read
 */
int sfs_fread(int fileID, char *buf, int length) {

    char *temp_buf = NULL;
    int len_temp_buf = 0;

    // make sure this is an open file
    if (fdt[fileID].used == 0) {
        return 0;
    }


    // the the file descriptor and the inode of the file
    file_descriptor *f = &fdt[fileID];
    inode_t *n = &inode_table[f->inode];

    //make sure you dont read pass the edn of file
    if (f->rwptr+length > n->size){
        length = n->size - f->rwptr;
    }

    // how many blocks to read
    int num_block_read = ((f->rwptr % BLOCK_SZ) + length) / BLOCK_SZ + 1;

    char buffer[num_block_read][BLOCK_SZ];

    // find in which block is the RWPTR
    uint64_t data_ptr_index = f->rwptr / BLOCK_SZ;


    /****************************************************************/
    /********************** read the blocks *************************/
    /****************************************************************/
    int i;
    for (i = 0; i < num_block_read; i++) {

        // if the block to read is in the direct pointer
        if (data_ptr_index < 12) {
            read_blocks(n->data_ptrs[data_ptr_index], 1, buffer[i]);
        }

            // if the block to read is in the indirect pointer
        else {
            //if indirect pointer exist
            if (n->indirect_ptrs != -1) {
                indirect_t* indirect_pointer = malloc(sizeof(indirect_t));
                read_blocks(n->indirect_ptrs, 1, (void*) indirect_pointer);
                read_blocks(indirect_pointer->data_ptr[data_ptr_index - 12], 1, buffer[i]);
            } else {
                printf("SFS > ERROR While reading the file");
            }
        }
        data_ptr_index++;
    }



    /***************************************************************/
    /******* put all the blocks in the output buffer ***************/
    /***************************************************************/

    int start_ptr = f->rwptr % BLOCK_SZ;
    int end_ptr = (start_ptr + length) % BLOCK_SZ;

    int j;

    // one block
    if (num_block_read == 1) {
        for (j = start_ptr; j < end_ptr; j++) {
            temp_buf = appendCharToCharArray(temp_buf, buffer[0][j], len_temp_buf);
            len_temp_buf++;
        }


        // two blocks
    } else if (num_block_read == 2) {
        for (j = start_ptr; j < BLOCK_SZ; j++) {
            temp_buf = appendCharToCharArray(temp_buf, buffer[0][j], len_temp_buf);
            len_temp_buf++;
        }
        for (j = 0; j < end_ptr; j++) {
            temp_buf = appendCharToCharArray(temp_buf, buffer[1][j], len_temp_buf);
            len_temp_buf++;
        }


        // more than two blocks
    } else {
        for (j = start_ptr; j < BLOCK_SZ; j++) {
            temp_buf = appendCharToCharArray(temp_buf, buffer[0][j], len_temp_buf);
            len_temp_buf++;
        }

        for (j = 1; j < num_block_read - 1; j++) {
            int k;
            for (k = 0; k < BLOCK_SZ; k++) {
                temp_buf = appendCharToCharArray(temp_buf, buffer[j][k], len_temp_buf);
                len_temp_buf++;
            }
        }

        for (j = 0; j < end_ptr; j++) {
            temp_buf = appendCharToCharArray(temp_buf, buffer[num_block_read - 1][j], len_temp_buf);
            len_temp_buf++;
        }
    }


    f->rwptr += length;

    for(i = 0; i < length; i++) {
        buf[i] = temp_buf[i];
    }


	return len_temp_buf;
}



/**
 * @brief Write some data to the a file
 * @param int File ID of an open file
 * @param const char Data that need to be written
 * @oaram int Length of the data
 * @retval int The number of bytes written
 */
int sfs_fwrite(int fileID, const char *buf, int length){

    int count = 0;
    int length_left = length;


	// get the file descritor and the inode of the file
    file_descriptor* f = &fdt[fileID];
    inode_t* n = &inode_table[f->inode];


    /****************************************************************/
    /****************** find empty data_prt *************************/
    /****************************************************************/

    // check if there is any empty direct pointer first
    int ptr_index = 0;
    while (n->data_ptrs[ptr_index] != -1 && ptr_index < 12) {
        ptr_index++;
    }
    int ind_ptr_index = 0;


    // if no direct pointer is empty check the indirect pointer
    // if indirect pointer exist
    if (ptr_index == 12 && n->indirect_ptrs != -1) {
        indirect_t *indirect_pointer = malloc(sizeof(indirect_t));
        read_blocks(n->indirect_ptrs, 1, (void *) indirect_pointer);

        while (indirect_pointer->data_ptr[ind_ptr_index] != -1) {
            ind_ptr_index++;
            if(ind_ptr_index == NUM_INDIRECT){
                return 0;
            }
        }

        //if indirect pointer does not exist
    } else if (ptr_index == 12) {
        indirect_t *indirect_pointer = malloc(sizeof(indirect_t));
        for (ind_ptr_index = 0; ind_ptr_index < NUM_INDIRECT; ind_ptr_index++) {
            indirect_pointer->data_ptr[ind_ptr_index] = -1;
        }
        n->indirect_ptrs = get_index();
        write_blocks(n->indirect_ptrs, 1, (void *) indirect_pointer);
        ind_ptr_index = 0;
    }


    // if the pointer is in the middle of a block
    // you need to read this block, fill it up, and write it back
    if(f->rwptr%BLOCK_SZ != 0) {

    /*****************************************************************/
    /********* read_buffer to complete the last used block ***********/
    /*****************************************************************/

        char *read_buf = malloc(sizeof(char)*BLOCK_SZ);

        uint64_t init_rwptr = f->rwptr;

        sfs_fseek(fileID, (f->rwptr - (f->rwptr % BLOCK_SZ)));


        // read the block to which we want to append
        sfs_fread(fileID, read_buf, (init_rwptr % BLOCK_SZ));

        int len_read_buf = (init_rwptr % BLOCK_SZ);

        // complete the first block to be written with old and new stuff
        int i;

        if (length_left > BLOCK_SZ - (f->rwptr % BLOCK_SZ)) {
            for (i = 0; i < BLOCK_SZ - (f->rwptr % BLOCK_SZ); i++) {
                read_buf = appendCharToCharArray(read_buf, buf[i], len_read_buf);
                len_read_buf++;
                count++;
            }
        } else {
            for (i = 0; i < length_left; i++) {
                read_buf = appendCharToCharArray(read_buf, buf[i], len_read_buf);
                len_read_buf++;
                count++;
            }
        }





        /****************************************************************/
        /****************** write the first block ***********************/
        /****************************************************************/

        // if an direct pointer was found
        if (ptr_index < 12) {
            write_blocks(n->data_ptrs[ptr_index-1], 1, (void *) read_buf);
        }
        else if(ind_ptr_index == 0){
            write_blocks(n->data_ptrs[ptr_index-1], 1, (void *) read_buf);
        }

        // if there was no empty direct pointer
        else {
            if(n->indirect_ptrs != -1) {
                indirect_t *indirect_pointer = malloc(sizeof(indirect_t));
                read_blocks(n->indirect_ptrs, 1, (void *) indirect_pointer);
                write_blocks(indirect_pointer->data_ptr[ind_ptr_index-1], 1, (void *) read_buf);
            }
        }

        // update how many bit are left to right
        length_left -= BLOCK_SZ - (f->rwptr % BLOCK_SZ);
    }


    /****************************************************************/
    /************ write all the other blocks needed *****************/
    /****************************************************************/

    int k;
    char *write_buf;
    int len_write_buf;
    int num_block_left;
    if(length_left > 0) {
        num_block_left = (length_left - 1) / BLOCK_SZ + 1;
    } else if (length_left == 1) {
        num_block_left = 1;
    } else {
        num_block_left = 0;
    }

    for(k = 0; k < num_block_left; k++) {
        write_buf = "";
        len_write_buf = 0;


        /********** if this is a middle block, ie. complete block **********/
        if(length_left > BLOCK_SZ) {

            //make the block
            int w;
            for(w = length - length_left; w < length - length_left + BLOCK_SZ; w++){
                write_buf = appendCharToCharArray(write_buf, buf[w], len_write_buf);
                len_write_buf++;
                count++;
            }


            //write the block
            // direct pointer
            if(ptr_index < 12) {
                n->data_ptrs[ptr_index] = get_index();
                write_blocks(n->data_ptrs[ptr_index], 1, (void*) write_buf);
                ptr_index++;


            // indirect pointer
            } else {

                //exist
                if(n->indirect_ptrs != -1) {
                    indirect_t* indirect_pointer = malloc(sizeof(indirect_t));
                    read_blocks(n->indirect_ptrs, 1, (void*) indirect_pointer);
                    indirect_pointer->data_ptr[ind_ptr_index] = get_index();
                    write_blocks(n->indirect_ptrs, 1, (void*) indirect_pointer);
                    write_blocks(indirect_pointer->data_ptr[ind_ptr_index], 1, (void*) write_buf);
                    ind_ptr_index++;

                //does not exit
                } else{

                    //create indirect pointer
                    indirect_t* indirect_pointer = malloc(sizeof(indirect_t));
                    for(ind_ptr_index = 0; ind_ptr_index < NUM_INDIRECT; ind_ptr_index++){
                        indirect_pointer->data_ptr[ind_ptr_index] = -1;
                    }
                    n->indirect_ptrs = get_index();
                    write_blocks(n->indirect_ptrs, 1, (void*) indirect_pointer);
                    ind_ptr_index = 0;

                    //save block
                    indirect_pointer->data_ptr[ind_ptr_index] = get_index();
                    write_blocks(n->indirect_ptrs, 1, (void*) indirect_pointer);
                    write_blocks(indirect_pointer->data_ptr[ind_ptr_index], 1, (void*) write_buf);
                    ind_ptr_index++;
                }
            }

            length_left -= BLOCK_SZ;
        }



        /********** if this is the last block to be written **********/
        else {
            //make the block
            int w;
            for(w = length - length_left; w < length; w++){
                write_buf = appendCharToCharArray(write_buf, buf[w], len_write_buf);
                len_write_buf++;
                count++;
            }


            //write the block
            //direct pointer
            if(ptr_index < 12) {
                n->data_ptrs[ptr_index] = get_index();
                write_blocks(n->data_ptrs[ptr_index], 1, (void*) write_buf);
                ptr_index++;

                //indirect pointer
            } else {

                //exist
                if(n->indirect_ptrs != -1) {
                    indirect_t* indirect_pointer = malloc(sizeof(indirect_t));
                    read_blocks(n->indirect_ptrs, 1, (void*) indirect_pointer);
                    indirect_pointer->data_ptr[ind_ptr_index] = get_index();
                    write_blocks(n->indirect_ptrs, 1, (void*) indirect_pointer);
                    write_blocks(indirect_pointer->data_ptr[ind_ptr_index], 1, (void*) write_buf);
                    ind_ptr_index++;
                }

                //does not exist
                else{
                    //create indirect pointer
                    indirect_t* indirect_pointer = malloc(sizeof(indirect_t));
                    for(ind_ptr_index = 0; ind_ptr_index < NUM_INDIRECT; ind_ptr_index++){
                        indirect_pointer->data_ptr[ind_ptr_index] = -1;
                    }
                    n->indirect_ptrs = get_index();
                    write_blocks(n->indirect_ptrs, 1, (void*) indirect_pointer);
                    ind_ptr_index = 0;

                    //save block
                    indirect_pointer->data_ptr[ind_ptr_index] = get_index();
                    write_blocks(n->indirect_ptrs, 1, (void*) indirect_pointer);
                    write_blocks(indirect_pointer->data_ptr[ind_ptr_index], 1, (void*) write_buf);
                    ind_ptr_index++;
                }
            }
        }
    }


    // update file descriptor
    f->rwptr += length;

    // update bitmap
    uint8_t* free_bit_map = get_bitmap();
    write_blocks(NUM_BLOCKS - 1, 1, (void*) free_bit_map);

    // update inode
    n->size += length;
    write_blocks(1, (int) sb.inode_table_len, (void*) inode_table);

    return count;
}



/**
 * @brief Move the read write pointer of a file to a particular position
 * @param int File ID of an open file
 * @param int Location where the pointer need to be relocated
 * @retval int Return zero if successful
 */
int sfs_fseek(int fileID, int loc){

    // error checking 
    if(loc < 0 || loc > MAX_RWPTR){
        printf("SFS > Wrong RW location!\n");
    }
	
    fdt[fileID].rwptr = loc;
	return 0;
}



/**
 * @brief Remove a file from the file system
 * @param char Name of the file to be removed
 * @retval int Return zero if successful
 */
int sfs_remove(char *file) {

    // check if it is open
    int inode = -1;
    int i;
    int directory_table_index;
    for(i = 0; i < sizeof(directory_table)/sizeof(directory_table[0]); i++) {
        if(directory_table[i].used == 1) {

            if (strcmp(directory_table[i].name, file) == 0) {
                inode = directory_table[i].inode;
                directory_table_index = i;
            }
        }
    }

    if(inode == -1) {
        printf("SFS > File not found!\n");
        return -1;
    }

    // free bitmap
    inode_t* n = &inode_table[inode];
    int j = 0;
    while(n->data_ptrs[j] != -1 && j< 12){
        rm_index(n->data_ptrs[j]);
        j++;
    }
    j = 0;
    if(n->indirect_ptrs != -1){
        indirect_t* indirect_pointer = malloc(sizeof(indirect_t));
        read_blocks(n->indirect_ptrs, 1, (void*) indirect_pointer);
        while(indirect_pointer->data_ptr[j] != -1 && j< NUM_INDIRECT){
            rm_index(indirect_pointer->data_ptr[j]);
            j++;
        }
    }

    // remove from directory_table
    directory_table[directory_table_index].used = 0;

    // remove from inode_table
    inode_table[inode].used = 0;

    // update disk
    uint8_t* free_bit_map = get_bitmap();
    write_blocks(NUM_BLOCKS - 1, 1, (void*) free_bit_map);
    write_blocks(1, sb.inode_table_len, (void*) inode_table);
    write_blocks(sb.inode_table_len + 1, NUM_DIR_BLOCKS, (void*) directory_table);

	return 0;
}
