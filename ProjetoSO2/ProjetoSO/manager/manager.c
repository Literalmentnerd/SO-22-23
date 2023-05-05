#include "logging.h"
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>

#define PATHNAME ".pipe"
#define MESSAGELENGTH 289
#define MESSAGELISTLENGTH 257
#define PIPELENGTH 256
#define BOXNAME 32
#define BUFFER_SIZE 1029

static void print_usage() {
    fprintf(stderr, "usage: \n"
                    "   manager <register_pipe_name> create <box_name>\n"
                    "   manager <register_pipe_name> remove <box_name>\n"
                    "   manager <register_pipe_name> list\n");
}

void send_msg(int tx, char const *str) {
    size_t len = strlen(str);
    size_t written = 0;

    while (written < len) {
        ssize_t ret = write(tx, str + written, len - written);
        if (ret < 0) {
            fprintf(stderr, "[ERR]: write failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        written += (size_t) ret;
    }
}

void slice(const char *str, char *result, size_t start, size_t end)
{
    memcpy(result, str + start, end - start);
}

int main(int argc, char **argv) {
    char * register_pipename ;
    char pipename[PIPELENGTH];
    register_pipename = malloc(sizeof(char)*strlen(argv[1]));
    strcpy(register_pipename,argv[1]);
    strcat(register_pipename, PATHNAME);
    strcpy(pipename,argv[2]);
    int pipenamelength = (int) strlen(argv[2]);
    pipename[pipenamelength] = '\0';
    if (argc == 4) {
        int rx = open(register_pipename, O_WRONLY);
        uint8_t code = 7;
        char message[MESSAGELISTLENGTH];
        char ccode = (char) code;
        memcpy(message,&ccode, 1);
        memcpy(message, pipename, PIPELENGTH);
        send_msg(rx, message);
        close(rx);
    } else if(argc == 5){
        char * namefour;
        namefour = malloc(sizeof(char)*strlen(argv[4]));
        char box_name[BOXNAME];
        strcpy(box_name,argv[4]);
        int boxnamelength = (int) strlen(argv[4]);
        box_name[boxnamelength] = '\0';
        int rx = open(register_pipename, O_WRONLY);
        if(strcmp(namefour, "create") == 0) {
            uint8_t code = 3;
            char message[MESSAGELENGTH];
            char ccode = (char) code;
            memcpy(message,&ccode, 1);
            memcpy(message, pipename, PIPELENGTH);
            memcpy(message, box_name, BOXNAME);
            send_msg(rx, message);
        }else{
            uint8_t code = 5;
            char message[MESSAGELENGTH];
            char ccode = (char) code;
            memcpy(message,&ccode, 1);
            memcpy(message, pipename, PIPELENGTH);
            memcpy(message, box_name, BOXNAME);
            send_msg(rx, message);
        }
        close(rx);
    } else{
        print_usage();
    }
    int tx = open(pipename, O_RDONLY);
    for (;;) {
        char buffer[BUFFER_SIZE];
        ssize_t ret = read(tx, buffer, BUFFER_SIZE - 1);
        if (ret > 0) {
            buffer[ret] = 0;
            char ccode1;
            slice(buffer, &ccode1, 0, 1);
            uint8_t code1 = (uint8_t) atoi(&ccode1);
            if (code1 == 4) {
                char ccode2[4];
                slice(buffer, ccode2, 1, 5);
                uint32_t code2 = (uint32_t) atoi(ccode2);
                if (code2 == 0){
                    fprintf(stdout, "OK\n");
                }else{
                    char errormessage[1024];
                    slice(buffer, errormessage, 5, 1029);
                    fprintf(stdout, "ERROR %s\n", errormessage);
                }
                return 0;
            } else if (code1 == 6){
                char ccode2[4];
                slice(buffer, ccode2, 1, 5);
                uint32_t code2 = (uint32_t) atoi(ccode2);
                if (code2 == 0){
                    fprintf(stdout, "OK\n");
                }else{
                    char errormessage[1024];
                    slice(buffer, errormessage, 5, 1029);
                    fprintf(stdout, "ERROR %s\n", errormessage);
                }
                return 0;
            } else if(code1 == 8){
                char clast;
                slice(buffer, &clast, 1, 2);
                uint8_t last = (uint8_t) atoi(&clast);
                char box_name[32];
                slice(buffer, box_name, 2, 34);
                char sbox_size[8];
                slice(buffer, sbox_size, 34, 42);
                uint64_t box_size = (uint64_t) atoi(sbox_size);
                char sn_publishers[8];
                slice(buffer, sn_publishers, 42, 50);
                uint64_t n_publishers = (uint64_t) atoi(sn_publishers);
                char sn_subscribers[8];
                slice(buffer, sn_subscribers, 50, 58);
                uint64_t n_subscribers = (uint64_t) atoi(sn_subscribers);
                fprintf(stdout, "%s %zu %zu %zu\n", box_name, box_size, n_publishers, n_subscribers);
                if (last == 1) {
                    return 0;
                }
            }
        }
    }
}
