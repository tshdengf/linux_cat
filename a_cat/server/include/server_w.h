#ifndef _MSG_H_
#define _MSG_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <errno.h>
#include <signal.h>

#include <pthread.h>
#include <sqlite3.h>
#include <unistd.h>

#include <sys/epoll.h>
#include "cJSON.h"


#define EVENTS_SIEZ 1024
// #define RECVBUF_SIZE    65536       //64K
#define RECVBUF_SIZE    100       //64K
#define INT_SIZE 4

struct callback
{
    FILE *fp;
    char name[20];
    char toname[20];
};

struct online
{
    int cfd;
    int say;
    char name[20];
    struct online *next;
};

typedef struct oooo
{
    int cfd;
    int use;
    int offset;
    char filename[20];
}stro_sockfd;

typedef struct messgae
{
    int action;
    int sfile_id;
    int filesize;
    int count;
    int bs;
    int offset;
    long cfd;
    char name[20];
    char password[20];
    char toname[20];
    char filename[20];
    char message[1024];
    char now_time[32];
}Msg;
stro_sockfd find_cfd[20];


int Create_sock();
void createlist();
void *thread_read(void *arg);
void server_file();//端口号先写死5858.                
int query_manager_password(Msg *msg);
cJSON * getcJSON();



#endif // !_MSG_H_