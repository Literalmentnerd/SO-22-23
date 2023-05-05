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
#define PIPELENGTH 256
#define BOXNAME 32 
#define BUFFER_SIZE 1025

int n_messages = 0;

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

static void sigint_handler() {
    fprintf(stdout, "Caught SIGINT - that's all folks!\n");
    fprintf(stdout, "%d", n_messages);
    exit(EXIT_SUCCESS);
}

void slice(const char *str, char *result, size_t start, size_t end)
{
    memcpy(result, str + start, end - start);
}

int main(int argc, char **argv) {
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        exit(EXIT_FAILURE);
    }
    if (argc != 4) {
        fprintf(stderr, "error\n");
    }
    char * register_pipename ;
    char pipename[PIPELENGTH];
    char box_name[BOXNAME];
    register_pipename = malloc(sizeof(char)*strlen(argv[1]));
    strcpy(register_pipename,argv[1]);
    strcat(register_pipename, PATHNAME);
    strcpy(pipename,argv[2]);
    int pipenamelength = (int) strlen(argv[2]);
    pipename[pipenamelength] = '\0';
    strcpy(box_name,argv[3]);
    int boxnamelength = (int) strlen(argv[3]);
    box_name[boxnamelength] = '\0';
    int rx = open(register_pipename, O_WRONLY);
    // [ code = 2 (uint8_t) ] | [ client_named_pipe_path (char[256]) ] | [ box_name (char[32]) ]
    uint8_t code = 2;
    char message[MESSAGELENGTH];
    char ccode = (char) code;
    memcpy(message,&ccode, 1);
    memcpy(message, pipename, PIPELENGTH);
    memcpy(message, box_name, BOXNAME);
    send_msg(rx, message);
    close(rx);
    int tx = open(pipename, O_RDONLY);
    for (;;) { // Loop forever, waiting for signals
        char buffer[BUFFER_SIZE];
        ssize_t ret = read(tx, buffer, BUFFER_SIZE - 1);
        if (ret > 0) {
            buffer[ret] = 0;
            char ccode1;
            char message1[1024];
            slice(buffer, &ccode1, 0, 1);
            uint8_t code1 = (uint8_t) atoi(&ccode1);
            slice(buffer, message1, 1, 1025);
            if (code1 == 10) {
                fprintf(stdout, "%s\n", message1);
            }
        }
    }
    fprintf(stderr, "usage: sub <register_pipe_name> <box_name>\n");
    return -1;
}
