/*
Segundo Projecto de SO 2022/2023
Realizado pelo Grupo 7 do turno L07
Grupo constituido por:
Artur Vasco Martins ist1102503
Bernardo Augusto ist1102820
*/
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
#include "operations.h"
#include "state.h"
#include "producer-consumer.h"


#define PATHNAME ".pipe"
#define BUFFER_SIZE (289)
#define MESSAGE_SIZE (1025)
#define BOXCREATERESPONSE 1029
#define PIPENAME_SIZE 256
#define BOXNAME_SIZE 32
#define ERROR_MESSAGE_SIZE 1024
#define LIST_MESSAGE_SIZE 58
#define BOX_SIZE_BITS 1024
static tfs_params fs_params;
#define INODE_TABLE_SIZE (fs_params.max_inode_count)

typedef struct
{
    char boxname[BOXNAME_SIZE];
    char pipename[PIPENAME_SIZE];
    int i; //0 se publisher 1 se subscriber
}box;


long unsigned int max_sessions = 0;
int sessions = 0;
pthread_t *tid;
box *userarray;
char **boxarray;
pthread_mutex_t userarraylock = PTHREAD_MUTEX_INITIALIZER;
pc_queue_t queue;
int n_boxes = 0;
pthread_mutex_t *thread_locks;
pthread_cond_t *cond; 
int *threads_used;

static void sigint_handler() {
    fprintf(stdout, "Caught SIGINT - that's all folks!\n");
    pcq_destroy(&queue);
    for(int a = 0; a < max_sessions; a++) {
        pthread_join(tid[a], NULL);
    }
    free(tid);
    free(userarray);
    for(int b = 0; b < INODE_TABLE_SIZE; b++) {
        free(boxarray[b]);
    }
    free(boxarray);
    pthread_mutex_destroy(&userarraylock);
    for(int c = 0; c < max_sessions; c++) {
        pthread_mutex_destroy(&thread_locks[c]);
    }
    free(thread_locks);
    for(int d = 0; d < max_sessions; d++) {
        pthread_cond_destroy(&cond[d]);
    }
    free(cond);
    free(threads_used);
    exit(EXIT_SUCCESS);
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

void add_box(char * boxname) {
    for(int i = 0; i < INODE_TABLE_SIZE; i++) {
        if(boxarray[i][0] == '\0') {
            memcpy(boxarray[i], boxname, BOXNAME_SIZE);
            break; //precisamos de parar o for senao todas as caixas ficam iguais
        }
    }
}

void remove_box_boxarray(char * boxname) {
    for(int i = 0; i < INODE_TABLE_SIZE; i++){
        if (memcmp(boxarray[i], boxname, BOXNAME_SIZE) == 0){
            boxarray[i][0] = '\0';
        }
    }
}

char *convert(uint8_t *a)
{
  char* buffer2;
  int i;

  buffer2 = malloc(9);
  if (!buffer2)
    return NULL;

  buffer2[8] = 0;
  for (i = 0; i <= 7; i++)
    buffer2[7 - i] = (((*a) >> i) & (0x01)) + '0';

  puts(buffer2);

  return buffer2;
}

void slice(const char *str, char *result, size_t start, size_t end)
{
    memcpy(result, str + start, end - start);
}

void *threadfunction(void* data){
    char message[BUFFER_SIZE];
    const int thread_number = *((int*)data);
    pthread_mutex_lock(&thread_locks[thread_number]);
    void * dequeued = pcq_dequeue(&queue);
    while (dequeued == NULL){
        pthread_cond_wait(&cond[thread_number], &thread_locks[thread_number]);
        dequeued = pcq_dequeue(&queue);
    }
    pthread_mutex_unlock(&thread_locks[thread_number]);
    threads_used[thread_number] = 1;
    memcpy(message,dequeued, BUFFER_SIZE);
    char ccode;
    char client_pipename[PIPENAME_SIZE];
    char box_name[BOXNAME_SIZE];
    slice(message, &ccode, 0, 1);
    uint8_t code = (uint8_t) atoi(&ccode);
    slice(message, client_pipename, 1, 257);
    slice(message, box_name, 257, 289);
    if (code == 1){
        while(true){
            int rx = open(client_pipename, O_RDONLY);
            char buffer[MESSAGE_SIZE];
            ssize_t ret = read(rx, buffer, MESSAGE_SIZE - 1);
            buffer[ret] = 0;
            if (ret > 0){
                char ccode1;
                char message1[ERROR_MESSAGE_SIZE];
                slice(buffer, &ccode1, 0, 1);
                uint8_t code1 = (uint8_t) atoi(&ccode1);
                slice(buffer, message1, 1, 1025);
                char file[33];
                memcpy(file, "/", 1);
                memcpy(file, box_name, 32);
                if (code1 == 9) {
                    int i = tfs_open(file, TFS_O_APPEND);
                    tfs_write(i, message1, 1024);
                    tfs_close(i);
                }
            }
            close(rx);
        }
    } else{
        while(true){
            int rx = open(client_pipename, O_WRONLY);
            char buffer[ERROR_MESSAGE_SIZE];
            char file[33];
            memcpy(file, "/", 1);
            memcpy(file, box_name, 32);
            int a = tfs_open(file, TFS_O_APPEND);
            while(tfs_read(a, buffer, ERROR_MESSAGE_SIZE) > 0) {
                char message1[MESSAGE_SIZE];
                uint8_t code1 = 10;
                char ccode1 = (char) code1;
                memcpy(message1,&ccode1, 1);
                memcpy(message1, buffer, ERROR_MESSAGE_SIZE);
                send_msg(rx, message1);
            }
            tfs_close(a);
            close(rx);
        }

    }
    threads_used[thread_number] = 0;
    return NULL;
}

void register_publisher(char * pipename, char * boxname, char * buffer){
    pthread_mutex_lock(&userarraylock);
    char file[33];
    memcpy(file, "/", 1);
    memcpy(file, boxname, 32);
    int xi = tfs_open(file, TFS_O_APPEND);
    if(xi == -1){
        fprintf(stderr, "Erro\n");
        pthread_mutex_unlock(&userarraylock);
        return;
    } else{
        tfs_close(xi);
        for (int a = 0; a < max_sessions ; a++) {
            if(memcmp(userarray[a].boxname, boxname, BOXNAME_SIZE) == 0 && userarray[a].i == 0){
                fprintf(stderr,"Erro, Box já está associada com um Publisher");
                pthread_mutex_unlock(&userarraylock);
                return;
            }
        }
        userarray[sessions].i = 0;
        memcpy(userarray[sessions].boxname, boxname, BOXNAME_SIZE);
        memcpy(userarray[sessions].pipename, pipename, PIPENAME_SIZE);
        pcq_enqueue(&queue, buffer);
        for (int d = 0; d < max_sessions; d++) {
            if (threads_used[d] == 0) {
                pthread_cond_signal(&cond[d]);
                break;
            }
        }
        sessions++;
    }
    pthread_mutex_unlock(&userarraylock);

}

void register_subscriber(char * pipename, char * boxname, char * buffer){
    pthread_mutex_lock(&userarraylock);
    char file[33];
    memcpy(file, "/", 1);
    memcpy(file, boxname, 32);
    int xi = tfs_open(file, TFS_O_APPEND);
    if(xi == -1){
        fprintf(stderr, "Erro\n");
        pthread_mutex_unlock(&userarraylock);
        return;
    } else{
        tfs_close(xi);
        userarray[sessions].i = 1;
        memcpy(userarray[sessions].boxname, boxname, BOXNAME_SIZE);
        memcpy(userarray[sessions].pipename, pipename, PIPENAME_SIZE);
        pcq_enqueue(&queue, buffer);
        for (int d = 0; d < max_sessions; d++) {
            if (threads_used[d] == 0) {
                pthread_cond_signal(&cond[d]);
                break;
            }
        }
        sessions++;
    }
    pthread_mutex_unlock(&userarraylock);
}

void create_box(char * pipename, char * boxname) {
    int32_t return_code;
    char error_message[ERROR_MESSAGE_SIZE];
    char file[33];
    memcpy(file, "/", 1);
    memcpy(file, boxname, 32);
    return_code = 0;
    int i = tfs_open(file, TFS_O_CREAT);
    if(i != -1) {
        memcpy(error_message, "Caixa existe", 13);
        return_code = -1;
    } else {
        error_message[0] = '\0';
        add_box(boxname);
        n_boxes++;
    }
    int rx = open(pipename, O_WRONLY);
    char creturn_code[4];
    sprintf( creturn_code, "%I64u", return_code);
    uint8_t code = 4;
    char *ccode = convert(&code);
    char message[BOXCREATERESPONSE];
    memcpy(message,ccode, 1);
    memcpy(message, creturn_code, 4);
    memcpy(message, error_message, ERROR_MESSAGE_SIZE);
    send_msg(rx, message);
    close(rx);
}

void remove_box(char * pipename, char * boxname) {
    int32_t return_code;
    char error_message[ERROR_MESSAGE_SIZE];
    char file[33];
    memcpy(file, "/", 1);
    memcpy(file, boxname, 32);
    int i = tfs_open(file, TFS_O_CREAT);
    if(i == -1) {
        memcpy(error_message, "Caixa nao existe", 13);
        return_code = -1;
    } else{
        return_code = 0;
        error_message[0] = '\0';
        if (tfs_unlink(file) == -1) {
            memcpy(error_message, "ERRO", 5);
        }else{
            remove_box_boxarray(boxname);
            n_boxes--;
            pthread_mutex_lock(&userarraylock);
            for (int a = 0; a < max_sessions ; a++) {
                if(memcmp(userarray[a].boxname, boxname, BOXNAME_SIZE) == 0){
                    userarray[a].boxname[0] = '\0';
                    userarray[a].pipename[0] = '\0';
                    userarray[a].i = -1;
                    sessions--;
                }
            }
            pthread_mutex_unlock(&userarraylock);
        }
    }
    int rx = open(pipename, O_WRONLY);
    char creturn_code[4];
    sprintf( creturn_code, "%I64u", return_code);
    uint8_t code = 6;
    char *ccode = convert(&code);
    char message[BOXCREATERESPONSE];
    memcpy(message,ccode, 1);
    memcpy(message, creturn_code, 4);
    memcpy(message, error_message, ERROR_MESSAGE_SIZE);
    send_msg(rx, message);
    close(rx);
}

int cmpfunc(const void *a, const void *b) {
    return strcmp(*(const char **)a, *(const char **)b);
}

void list_boxes(char * pipename){
    uint8_t code = 8;
    char *ccode = convert(&code);
    uint8_t last = 0;
    char namelist[n_boxes][BOXNAME_SIZE];
    int rx = open(pipename, O_WRONLY);
    uint64_t sub = 0;
    uint64_t pub = 0;
    uint64_t size = BOX_SIZE_BITS;
    char message[LIST_MESSAGE_SIZE];
    int n = 0;
    for(int i = 0; i<n_boxes;i++){
        for(int a = n; a < INODE_TABLE_SIZE; a++) {
            if(boxarray[a][0] != '\0'){
                memcpy(namelist[i], boxarray[a], BOXNAME_SIZE);
                n = a + 1;
                break;
            }
        }
    }
    qsort(namelist,(size_t) n_boxes,sizeof(char)* BOXNAME_SIZE,cmpfunc);
    for(int j=0; j<n_boxes;j++){
        for(int x=0; x<max_sessions;x++){
            if(namelist[j] == userarray[x].boxname){
                if(userarray[x].i == 0){
                    pub++;
                } else{
                    sub++;
                }
            }
        }
        if(j == n_boxes-1){
            last = 1;
        }
        char *llast = convert(&last);
        char *ssub = (char*)sub;
        char *ppub = (char*)pub;
        char *ssize = (char*)size;
        memcpy(message,ccode,1);
        memcpy(message,llast,1);
        memcpy(message,namelist[j],BOXNAME_SIZE);
        memcpy(message,ssize,8);
        memcpy(message,ppub,8);
        memcpy(message,ssub,8);
        send_msg(rx, message);
        sub = 0;
        pub = 0;
    }
    close(rx);
}

int main(int argc, char **argv) {
    if (signal(SIGINT, sigint_handler) == SIG_ERR) {
        exit(EXIT_FAILURE);
    }
    if(tfs_init(NULL) == -1){
        return -1;
    }
    if (argc != 3) {
        fprintf(stderr, "error\n");
    }
    char * register_pipename;        //register_pipename, pipe que recebe os pedidos de registo dos clientes 
    register_pipename = malloc(sizeof(char)*strlen(argv[1]));
    strcpy(register_pipename,argv[1]);
    strcat(register_pipename, PATHNAME);
    max_sessions = (long unsigned int)atoi(argv[2]);
    tid = malloc(max_sessions * sizeof(pthread_t));
    userarray = malloc(INODE_TABLE_SIZE * sizeof(box));
    boxarray = (char**)malloc(sizeof(char*)*INODE_TABLE_SIZE);
    thread_locks = malloc(sizeof(pthread_mutex_t) * max_sessions);
    cond = malloc(sizeof(pthread_cond_t)* max_sessions);
    threads_used = (int *) malloc(sizeof(int) * max_sessions);
    for (int gb = 0; gb < max_sessions; gb++) {
        threads_used[gb] = 0;
    }
    for (int g = 0; g < max_sessions; g++) {
        pthread_mutex_init(&thread_locks[g], NULL);
    }
    int threadvalues[max_sessions];
    for (int gk = 0; gk < max_sessions; gk++) {
        threadvalues[gk] = gk;
    }
    for (int gi = 0; gi < max_sessions; gi++) {
        pthread_cond_init(&cond[gi], NULL);
    }
    pcq_create(&queue, max_sessions);
    for(int i=0; i<INODE_TABLE_SIZE; i++)
    {
       boxarray[i] = (char*)malloc(sizeof(char)*BOXNAME_SIZE);
    }
    for(int a=0; a<INODE_TABLE_SIZE; a++)
    {
       boxarray[a][0] = '\0';
    }
    for(int i = 0; i < max_sessions; i++) {
        pthread_create(&tid[i], NULL, threadfunction, &threadvalues[i]);
    }
    for (int g = 0; g <  max_sessions; g++) {
        userarray[g].i = -1;
        userarray[g].boxname[0] = '\0';
        userarray[g].pipename[0] = '\0';
    }
    if (unlink(register_pipename) != 0 && errno != ENOENT) {
        fprintf(stderr, "[ERR]: unlink(%s) failed: %s\n", register_pipename,
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (mkfifo(register_pipename, 0640) != 0) {
        fprintf(stderr, "[ERR]: mkfifo failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    int rx = open(register_pipename, O_RDONLY);
    if (rx == -1) {
        fprintf(stderr, "[ERR]: open failed: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    while (true) {
        char buffer[BUFFER_SIZE];
        ssize_t ret = read(rx, buffer, BUFFER_SIZE - 1);
        if (ret == 0) {
            // ret == 0 indicates EOF
            fprintf(stderr, "[INFO]: pipe closed\n");
            return 0;
        } else if (ret == -1) {
            // ret == -1 indicates error
            fprintf(stderr, "[ERR]: read failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }
        buffer[ret] = 0;
        char *ccode;
        char client_pipename[PIPENAME_SIZE];
        char box_name[BOXNAME_SIZE];
        slice(buffer, ccode, 0, 1);
        uint8_t code = (uint8_t) atoi(ccode);
        slice(buffer, client_pipename, 1, 257);
        slice(buffer, box_name, 257, 289);
        switch (code){
        case(1):{
            register_publisher(client_pipename,box_name, buffer);
            break;
        };
        case(2):{
            register_subscriber(client_pipename,box_name, buffer);
            break;
        }
        case(3):{
            create_box(client_pipename, box_name);
            break;
        }
        case(5):{
            remove_box(client_pipename, box_name);
            break;
        }
        case(7):{
            list_boxes(client_pipename);
            break;
        }
        default:
            fprintf(stderr, "Erro\n");
        }
    }
    close(rx);
    fprintf(stderr, "usage: mbroker <pipename>\n");
    return -1;
}