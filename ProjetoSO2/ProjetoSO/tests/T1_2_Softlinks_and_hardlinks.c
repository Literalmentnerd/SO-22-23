#include "fs/operations.h"
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

char const target_path1[] = "/f1";
char const link_path1[] = "/l1";
char const link_path2[] = "/l2";

int main() {

    // init TécnicoFS
    assert(tfs_init(NULL) != -1);

    // create file with content
    {
        int f = tfs_open(target_path1, TFS_O_CREAT);
        assert(tfs_close(f) != -1);
    }

    // try to create hard link on a soft link
    assert(tfs_link(target_path1, link_path2) != -1);
    // create soft link on a file
    assert(tfs_sym_link(link_path2, link_path1) != -1);

    // Already existing hardlink
    assert(tfs_link(link_path2,link_path2) == -1);
    
    // Unlink file
    assert(tfs_unlink(link_path2) != -1);
    assert(tfs_unlink(link_path2) == -1);

    // Already existing softlink
    assert(tfs_sym_link(link_path1,link_path1) == -1);

    // Unlink file
    assert(tfs_unlink(link_path1) != -1);
    assert(tfs_unlink(link_path1) == -1);

    // destroy TécnicoFS
    assert(tfs_destroy() != -1);

    printf("Successful test.\n");

    return 0;
}
