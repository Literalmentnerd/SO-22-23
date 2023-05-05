#include "fs/operations.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>

char *str_ext_file = "BBB!";
char *path_copied_file = "/f1";
char *path_src = "tests/file_to_copy.txt";
char buffer[40];

char const link_path1[] = "/l1";
char const link_path2[] = "/l2";

void *thread1() {
    int f;
    ssize_t r;
    
    f = tfs_copy_from_external_fs(path_src,path_copied_file);
    assert(f != -1);

    f = tfs_open(path_copied_file,TFS_O_CREAT);
    assert(f != -1);

    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str_ext_file));
    assert(!memcmp(buffer, str_ext_file, strlen(str_ext_file)));

    f = tfs_copy_from_external_fs(path_src, path_copied_file);
    assert(f != -1);

    f = tfs_open(path_copied_file, TFS_O_CREAT);
    assert(f != -1);

    r = tfs_read(f, buffer, sizeof(buffer) - 1);
    assert(r == strlen(str_ext_file));
    assert(!memcmp(buffer, str_ext_file, strlen(str_ext_file)));

    return NULL;
}

void *thread2() {
    int f;

    f = tfs_copy_from_external_fs(path_src,path_copied_file);
    assert(f != -1);

    f = tfs_open(path_copied_file, TFS_O_CREAT);
    assert(f != -1);

    assert(tfs_sym_link(path_copied_file, link_path1) != -1);

    assert(tfs_link(link_path1, link_path2) == -1);

    return NULL;
}

void *thread3() {
    int f;

    f = tfs_copy_from_external_fs(path_src,path_copied_file);
    assert(f != -1);

    f = tfs_open(path_copied_file, TFS_O_CREAT);
    assert(f != -1);

    assert(tfs_sym_link(path_copied_file, link_path2) != -1);

    return NULL;
}

int main() {
    pthread_t threads[2];
    assert(tfs_init(NULL) != -1);
    pthread_create(&threads[0], NULL, thread1, NULL);
    pthread_create(&threads[1], NULL, thread2, NULL);
    pthread_create(&threads[2], NULL, thread3, NULL);
    for (int i = 0; i < 2; i += 1) {
        pthread_join(threads[i], NULL);
    }
    printf("Successful test.\n");
    return 0;
}