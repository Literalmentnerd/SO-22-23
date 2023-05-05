/**
 * Grupo 7 - 2022/2023
 * @file operations.c
 * @author Bernardo Augusto ist1102820 Artur Martins ist1102503
 * @brief 
 * @version 0.1
 * @date 2022-12-20
 * 
 * @copyright Copyright (c) 2022
 * 
 */
#include "operations.h"
#include "config.h"
#include "state.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "betterassert.h"

tfs_params tfs_default_params() {
    tfs_params params = {
        .max_inode_count = 64,
        .max_block_count = 1024,
        .max_open_files_count = 16,
        .block_size = 1024,
    };
    return params;
}


int tfs_init(tfs_params const *params_ptr) {
    tfs_params params;
        if (params_ptr != NULL) {
        params = *params_ptr;
    } else {
        params = tfs_default_params();
    }
    
    if (state_init(params) != 0) {
        return -1;
    }

    // create root inode
    int root = inode_create(T_DIRECTORY);
    if (root != ROOT_DIR_INUM) {
        return -1;
    }
    return 0;
}

int tfs_destroy() {
    if (state_destroy() != 0) {
        return -1;
    }
    return 0;
}

static bool valid_pathname(char const *name) {
    return name != NULL && strlen(name) > 1 && name[0] == '/';
}

/**
 * Looks for a file.
 *
 * Note: as a simplification, only a plain directory space (root directory only)
 * is supported.
 *
 * Input:
 *   - name: absolute path name
 *   - root_inode: the root directory inode
 * Returns the inumber of the file, -1 if unsuccessful.
 */
static int tfs_lookup(char const *name, inode_t const *root_inode) {
    if (!valid_pathname(name)) {
        return -1;
    }

    // skip the initial '/' character
    name++;

    return find_in_dir(root_inode, name);
}

int tfs_open(char const *name, tfs_file_mode_t mode) {
    // Checks if the path name is valid
    if (!valid_pathname(name)) {
        return -1;
    }
    inode_t *root_dir_inode = inode_get_secure(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                  "tfs_open: root dir inode must exist");             
    int inum = tfs_lookup(name, root_dir_inode);
    size_t offset;

    if (inum >= 0) {
        // The file already exists
        inode_t *inode = inode_get_secure(inum);
        ALWAYS_ASSERT(inode != NULL,
                      "tfs_open: directory files must have an inode");
        if(inode->is_shortcut){
            inum = tfs_lookup(inode->soft_link,root_dir_inode);
            if(inum == -1){
                return -1;
            }
            inode = inode_get_secure(inum);
        }
        // Truncate (if requested)
        if (mode & TFS_O_TRUNC) {
            inodereadlocker(inum);
            if (inode->i_size > 0) {
                data_block_free(inode->i_data_block);
                inodeunlocker(inum);
                inodewritelocker(inum);
                inode->i_size = 0;
                inodeunlocker(inum);
                inodereadlocker(inum);
            }
            inodeunlocker(inum);
        }
        // Determine initial offset
        if (mode & TFS_O_APPEND) {
            inodereadlocker(inum);
            offset = inode->i_size;
            inodeunlocker(inum);
        } else {
            offset = 0;
            
        }
    } else if (mode & TFS_O_CREAT) {
        // The file does not exist; the mode specified that it should be created
        // Create inode

        inum = inode_create(T_FILE);
        if (inum == -1) {
            return -1; // no space in inode table
        }

        // Add entry in the root directory
        if (add_dir_entry(root_dir_inode, name + 1, inum) == -1) {
            inode_delete(inum);
            return -1; // no space in directory
        }
        offset = 0;
    } else {
        return -1;
    }
    // Finally, add entry to the open file table and return the corresponding
    // handle
    return add_to_open_file_table(inum, offset);

    // Note: for simplification, if file was created with TFS_O_CREAT and there
    // is an error adding an entry to the open file table, the file is not
    // opened but it remains created
}

int tfs_sym_link(char const *target, char const *link_name) {
    //atalhos : soft link
    if(!valid_pathname(link_name)){
        return -1;
    }
    inode_t *root_dir_inode = inode_get_secure(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                "tfs_open: root dir inode must exist");
    int inum = tfs_lookup(target,root_dir_inode);
    if(inum < 0){
        return -1;
    }
    inum = tfs_lookup(link_name,root_dir_inode);
    if(inum != -1){
        return -1;
    }
    inum = inode_create(T_DIRECTORY);
    if(inum < 0){
        return -1;
    }
    inode_t* inode = inode_get_secure(inum);
    if(add_dir_entry(root_dir_inode, link_name+1, inum) == -1){
        return -1;
    } 
    inodewritelocker(inum);
    inode->is_shortcut = true;
    inode->soft_link = malloc(sizeof(char*));
    inodeunlocker(inum);
    memcpy(inode->soft_link,target,strlen(target)+1);
    return 0;
}

int tfs_link(char const *target, char const *link_name) {
    //nomes alternativos : hard link
    if(!valid_pathname(link_name)){
        return -1;
    }
    inode_t *root_dir_inode = inode_get_secure(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                "tfs_open: root dir inode must exist");
    int inum = tfs_lookup(target,root_dir_inode);
    if (inum < 0) {
        return -1;
    }
    int link_inum = tfs_lookup(link_name,root_dir_inode);
    if(link_inum != -1){
        return -1;
    }
    inode_t *inode = inode_get(inum);
    inodereadlocker(inum);
    if(inode->is_shortcut){
        inodeunlocker(inum);
        return -1;
    }
    if(add_dir_entry(root_dir_inode, link_name+1, inum) == -1){
        return -1; 
    }
    inodeunlocker(inum);
    inodewritelocker(inum); 
    inode->n_links++;
    inodeunlocker(inum);
    return 0;
}

int tfs_close(int fhandle) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1; // invalid fd
    }

    remove_from_open_file_table(fhandle);

    return 0;
}

ssize_t tfs_write(int fhandle, void const *buffer, size_t to_write) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    //  From the open file table entry, we get the inode
    inode_t *inode = inode_get_secure(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_write: inode of open file deleted");

    // Determine how many bytes to write
    size_t block_size = state_block_size();
    if (to_write + file->of_offset > block_size) {
        to_write = block_size - file->of_offset;
    }
    if (to_write > 0) {
        if (inode->i_size == 0) {
            // If empty file, allocate new block
            int bnum = data_block_alloc();
            if (bnum == -1) {
                return -1; // no space
            }

            inode->i_data_block = bnum;
        }

        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_write: data block deleted mid-write");

        // Perform the actual write
        memcpy(block + file->of_offset, buffer, to_write);

        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_write;
        if (file->of_offset > inode->i_size) {
            inode->i_size = file->of_offset;
        }
    }
    return (ssize_t)to_write;
}

ssize_t tfs_read(int fhandle, void *buffer, size_t len) {
    open_file_entry_t *file = get_open_file_entry(fhandle);
    if (file == NULL) {
        return -1;
    }

    // From the open file table entry, we get the inode
    inode_t const *inode = inode_get_secure(file->of_inumber);
    ALWAYS_ASSERT(inode != NULL, "tfs_read: inode of open file deleted");

    // Determine how many bytes to read
    size_t to_read = inode->i_size - file->of_offset;
    if (to_read > len) {
        to_read = len;
    }

    if (to_read > 0) {
        void *block = data_block_get(inode->i_data_block);
        ALWAYS_ASSERT(block != NULL, "tfs_read: data block deleted mid-read");

        // Perform the actual read
        memcpy(buffer, block + file->of_offset, to_read);
        // The offset associated with the file handle is incremented accordingly
        file->of_offset += to_read;
    }
    return (ssize_t)to_read;
}

int tfs_unlink(char const *target) {
    inode_t *root_dir_inode = inode_get_secure(ROOT_DIR_INUM);
    ALWAYS_ASSERT(root_dir_inode != NULL,
                "tfs_open: root dir inode must exist");
    int inum = tfs_lookup(target,root_dir_inode);
    if (inum < 0) {
        return -1;
    }
    inode_t *inode = inode_get(inum);
    inodereadlocker(inum);
    if(inode->is_shortcut){
        inode_delete(inum);
    } else {
        inodeunlocker(inum);
        inodewritelocker(inum);
        inode->n_links--;
        inodeunlocker(inum);
        inodereadlocker(inum);
        if(inode->n_links == 0){
            inode_delete(inum);
        }
    }
    inodeunlocker(inum);
    return clear_dir_entry(root_dir_inode,target+1); //versao funciona
}

int tfs_copy_from_external_fs(char const *source_path, char const *dest_path) {
    // ^ this is a trick to keep the compiler from complaining about unused
    // variables. TODO: remove
    
    int t = tfs_open(dest_path, TFS_O_CREAT);
    if (t<0){
        return -1;
    }
    FILE* imported = fopen(source_path, "r");
    if (imported == NULL) {
        tfs_close(t);
        return -1;
    }
    char buffer[128];
    memset(buffer,0,sizeof(buffer));
    size_t bytesread = fread(&buffer, sizeof(char), sizeof(buffer)/sizeof(char)-1, imported); 
    while (bytesread!= 0){
        if ((int)bytesread < 0){
            fclose(imported);
            tfs_close(t);
            return -1;
        }
        ssize_t byteswriten = tfs_write(t, buffer, bytesread);
        if (byteswriten < 0 || byteswriten != bytesread){
            fclose(imported);
            tfs_close(t);
            return -1;
        }
        memset(buffer,0,sizeof(buffer));

        /* read the contents of the file */
        bytesread = fread(&buffer, sizeof(char), sizeof(buffer)/sizeof(char)-1, imported);
    }
    if(fclose(imported) < 0){
        tfs_close(t);
        return -1;
    }

    if(tfs_close(t) < 0){
        return -1;
    }
    return 0;
}
