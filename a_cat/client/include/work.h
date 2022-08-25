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

#include <sys/mman.h>
#include <sys/epoll.h>
#include <semaphore.h>


#define EVENTS_SIEZ 1024
#define INT_SIZE 4
// #define SEND_SIZE    65536       	//64K
// #define BLOCKSIZE   134217728		//128M
// #define RECVBUF_SIZE    65536       //64K
#define SEND_SIZE    100       	//64K
#define BLOCKSIZE   500		//128M
#define RECVBUF_SIZE    100       //64K

struct conn
{
    int info_fd;       //信息交换socket：接收文件信息、文件传送通知client
    char filename[30]; //文件名
    int filesize;      //文件大小
    int bs;            //分块大小
    int count;         //分块数量
    int recvcount;     //已接收块数量，recv_count == count表示传输完毕
    char *mbegin;      // mmap起始地址
    int used;          //使用标记，1代表使用，0代表可用
};

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


int Create_sock();
void viewhome();
void viewlogin();
void viewchat();
void viewall();
void reg_user(int sockfd, Msg *pmsg);
void* thread_read(void *arg);
void log_user(int sockfd, Msg *pmsg);
void chat_user(int sockfd, Msg *pmsg);
void send_file(int sockfd, Msg *pmsg);
void all_user(int sockfd, Msg *pmsg);
void client_file(int port);
void all_manager(int sockfd, Msg *pmsg);
void log_manager(int sockfd, Msg *pmsg);
void view_user(int sockfd, Msg *pmsg);
void get_time(char *now_time);
void quite_user(int sockfd, Msg *pmsg);



#endif // !_MSG_H_