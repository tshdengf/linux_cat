#include "work.h"
#define SEM_SIZE 2

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_send = PTHREAD_MUTEX_INITIALIZER;
sem_t sem[SEM_SIZE];
pthread_cond_t cond_sfile = PTHREAD_COND_INITIALIZER;
extern int lognum;
char *mbegin;
int fork_num;
int sfile_id;
struct conn gconn[20];

void printf_stea(Msg *pmsg)
{
    char now_time[32] = {0};
    get_time(now_time);
    printf("时间%s,%s:%s\n",now_time,pmsg->name,pmsg->message);
    return;
}

void init_sem()
{
    for(int i=0; i<SEM_SIZE; i++)
    {
        if(i == 0)
        {
            sem_init(&sem[i], 0, 1);
        }
        else
        {
            sem_init(&sem[i], 0, 0);
        }
    }
    return;
}

//创建大小为size的文件
int createfile(char *filename, int size)
{
    int tmpfd = open(filename, O_RDWR | O_CREAT, 0655);
    fchmod(tmpfd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    ftruncate(tmpfd, size);
    close(tmpfd);
    return 0;
}

void *recv_file(void *arg)
{
    //线程独立
    pthread_t id = pthread_self();
    pthread_detach(id);
    sem_wait(&sem[1]);
    Msg omsg;
    memcpy(&omsg, (Msg *)arg, sizeof(Msg));
    sem_post(&sem[0]);
    int sockfd1 = Create_sock();
    //发送套接字
    omsg.action = -5;
    write(sockfd1, &omsg, sizeof(Msg));

    //准备接收
    char *fp = gconn[omsg.sfile_id].mbegin + omsg.offset;
    int ret = 0;

    while(1)
    {
        if((ret = (recv(sockfd1, fp, RECVBUF_SIZE, 0))) == RECVBUF_SIZE)
        {
            fp += RECVBUF_SIZE;
        }
        else if(ret < RECVBUF_SIZE)
        {
            break;
        }
    }
    close(sockfd1);
    return NULL;
}

void get_time(char *now_time)
{
    time_t now;
    struct tm *tm_now;
    time(&now);
    tm_now = localtime(&now);
    memset(now_time, 0, sizeof(now_time));
    sprintf(now_time, "%d-%d-%d %d:%d:%d", tm_now->tm_year + 1900, tm_now->tm_mon + 1, 
                        tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
    return;
}

void *thread_read(void *arg)
{
    init_sem();
    pthread_t id = pthread_self();
    pthread_detach(id);
    long cfd = (long)arg;

    Msg *pmsg = (Msg *)malloc(sizeof(Msg));
    
    while(1)
    {
        memset(pmsg,0,sizeof(Msg));
        int r_n = recv(cfd,pmsg,sizeof(Msg), MSG_WAITALL);
        if(r_n == 0)
        {
            printf("server exit!\n");
            pthread_exit(NULL);
        }

        switch(pmsg->action)
        {
            
            case 255:
            {
                printf("注册失败\n");
                break;
            }
            case 1:
            {
                lognum = 1;
                break;
            }
            case -1:
            {
                lognum = -1;
                break;
            }
            case 2:
            {
                printf_stea(pmsg);
                break;
            }
            case -2:
            {
                printf("未上线\n");
                break;
            }
            case 3:
            {
                sfile_id = pmsg->sfile_id;
                pthread_cond_signal(&cond_sfile);
                break;
            }
            case 4:
            {
                printf("%s对%s:%s\n",pmsg->name,pmsg->toname,pmsg->message);
                break;
            }
            case -3:
            {
                createfile("copy", pmsg->filesize);
                int fd;
                if ((fd = open("copy", O_RDWR)) == -1)
                {
                    printf("open file erro\n");
                    exit(-1);
                }
                char *map = (char *)mmap(NULL, pmsg->filesize, 
                                PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
                close(fd);
                gconn[pmsg->sfile_id].mbegin = map;
                gconn[pmsg->sfile_id].recvcount = 0;
                gconn[pmsg->sfile_id].count = pmsg->count;
                break;
            }
            case 5:
            {
                sem_wait(&sem[0]);
                Msg omsg;
                memcpy(&omsg, pmsg, sizeof(Msg));
                sem_post(&sem[1]);
                pthread_t id;
                if(pthread_create(&id, NULL, recv_file, (void*)&omsg) != 0)
                {
                    printf("%s:pthread_create failed, errno:%d, error:%s\n", __FUNCTION__, errno, strerror(errno));
                    exit(1);
                }
                break;
            }
            case 6:
            {
                char buf[100];
                FILE *fp = fopen("user.txt", "w+");
                if(fwrite(pmsg->message, strlen(pmsg->message), 1, fp) 
                                                        != sizeof(pmsg->message))
                {
                    fseek(fp, 0, SEEK_SET);
                    while(feof(fp) == 0)
                    {
                        fscanf(fp, "%s", buf);
                        printf("%s\n",buf);
                        memset(buf, 0, sizeof(buf));
                    }
                    fclose(fp);
                }
                break;
            }
            case 7:
            {
                printf("你被禁言了！\n");
                break;
            }
            case 8:
            {
                printf("禁言成功！\n");
                break;
            }
            case -8:
            {
                printf("解除禁言！\n");
                break;
            }
            case 9:
            {
                char buf[100];
                char buf2[100];
                FILE *fp = fopen("message.txt", "w+");
                while(fwrite(pmsg->message, strlen(pmsg->message), 1, fp) 
                                                        != sizeof(pmsg->message))
                {
                    fseek(fp, 0, SEEK_SET);
                    while(feof(fp) == 0)
                    {
                        // fscanf(fp, "%s %s", buf, buf2);
                        int i=0;
                        char ch;
                        fread(&ch, 1, 1, fp);
                        while(ch != '\n')
                        {
                            buf[i] = ch;
                            i++;
                            fread(&ch, 1, 1, fp);
                        }
                        printf("%s\n",buf);
                        memset(buf, 0, sizeof(buf));
                    }
                    fclose(fp);
                    break;
                }
                break;
            }
        }
    }

    return NULL;
}

/*******************************************
 ** 函数名：Create_sock
 ** 输  入：void
 ** 输  出：int sockfd
 ** 功能描述：返回已经连接的套接字
 ** 返回值类型：int
 *******************************************/
int Create_sock()
{
    int ret;
    int sockfd;
    struct sockaddr_in s_addr;

    sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd < 0)
    {
        perror("socket error!");
        exit(1);
    }

    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(5556);
    s_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    ret = connect(sockfd, (struct sockaddr *)&s_addr, sizeof(struct sockaddr_in));
    if(ret < 0)
    {
        perror("connect error!");
        exit(1);
    }

    return sockfd;
}

int Create_UDP(int port)
{
    int sockfd;
    struct sockaddr_in s_addr;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);
    s_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    bind(sockfd, (struct sockaddr*)&s_addr, sizeof(struct sockaddr_in));
    return sockfd;
}

void viewlogin()
{
    system("clear");
    printf("login\n");
    printf("1.登录\n");
    printf("2.注册\n");
    printf("3.管理员登录\n");
    printf("0.退出\n");
}

void view_user(int sockfd, Msg *pmsg)
{
    pmsg->action = 6;
    write(sockfd, pmsg, sizeof(Msg));
}

void viewhome(int sockfd, Msg *pmsg)
{
    system("clear");
    printf("home\n");
    printf("2.群聊\n");
    printf("1.私聊\n");
    printf("3.退出\n");  
}
/*******************************************
 ** 函数名：reg_user
 ** 输  入：int sockfd  接受对象的套接字
           Msg *pmsg
 ** 输  出：void
 ** 功能描述：向服务器发送用户名和密码
 ** 返回值类型：void
 *******************************************/
void reg_user(int sockfd, Msg *pmsg)
{
    memset(pmsg, 0, sizeof(Msg));
    pmsg->action = 0;
    printf("input name\n");
    scanf("%s",pmsg->name);

    printf("input password\n");
    scanf("%s",pmsg->password);

    if(write(sockfd, pmsg, sizeof(Msg)) < 0)
    {
        perror("write error!");
        exit(1);
    }
    return;
}

void log_user(int sockfd, Msg *pmsg)
{
    memset(pmsg, 0, sizeof(Msg));
    pmsg->action = 1;
    printf("input name\n");
    scanf("%s",pmsg->name);

    printf("input password\n");
    scanf("%s",pmsg->password);    

    if(write(sockfd, pmsg, sizeof(Msg)) < 0)
    {
        perror("write error!");
        exit(1);
    }
    return;
}

void viewchat()
{
    system("clear");
    printf("chat\n");
    printf("quite.退出\n"); 
    printf("sfile.发文件\n"); 
}

void chat_user(int sockfd, Msg *pmsg)
{
    printf("input toname\n");
    scanf("%s",pmsg->toname);
    viewchat();
    pmsg->action = 9;
    write(sockfd, pmsg, sizeof(Msg));
    while(1)
    {
        memset(pmsg->message, 0, sizeof(pmsg->message));
        scanf("%s",pmsg->message);
        if((0 == strcmp(pmsg->message, "sfile")))
        {
            send_file(sockfd, pmsg);
        }
        else if((0 == strcmp(pmsg->message,"quite")))
        {
            return;
        }
        else
        {
            pmsg->action = 2;
            if(write(sockfd,pmsg,sizeof(Msg)) < 0)
            {
                perror("write error!");
                exit(1);
            }
        }
        
    }
}

void viewall()
{
    system("clear");
    printf("all:\n");
    printf("1.群聊\n");
    printf("2.私聊\n");
    printf("0.退出\n"); 
}

void all_user(int sockfd, Msg *pmsg)
{
    strcpy(pmsg->toname, "all");
    viewall();

    while(1)
    {
        memset(pmsg->message, 0, sizeof(pmsg->message));
        scanf("%s", pmsg->message);
        if((0 == strcmp(pmsg->message, "sfile")))
        {

        }
        if((0 == strcmp(pmsg->message,"quite")))
        {
            return;
        }
        pmsg->action = 4;
        if(write(sockfd,pmsg,sizeof(Msg)) < 0)
        {
            perror("write error!");
            exit(1);
        }
    }
}

void quite_user(int sockfd, Msg *pmsg)
{
    pmsg->action = 10;
    write(sockfd, pmsg, sizeof(Msg));
    return;
}

void view_manager()
{
    system("clear");
    printf("all:\n");
    printf("quite.退出\n"); 
    printf("unable.禁言\n");
    printf("capable.解除禁言\n");
    printf("kick.踢人\n");

}

void all_manager(int sockfd, Msg *pmsg)
{
    strcpy(pmsg->toname, "all");
    view_manager();

    while(1)
    {
        memset(pmsg->message, 0, sizeof(pmsg->message));
        scanf("%s", pmsg->message);
        if((0 == strcmp(pmsg->message, "unable")))
        {
            pmsg->action = 8;
            printf("禁言的人是：\n");
            memset(pmsg->toname, 0, sizeof(pmsg->toname));
            scanf("%s",pmsg->toname);
            write(sockfd, pmsg, sizeof(Msg));
            
        }
        else if((0 == strcmp(pmsg->message, "capable")))
        {
            pmsg->action = -8;
            printf("解除禁言的人是：\n");
            memset(pmsg->toname, 0, sizeof(pmsg->toname));
            scanf("%s",pmsg->toname);
            write(sockfd, pmsg, sizeof(Msg));
            
        }
        else if((0 == strcmp(pmsg->message, "kick")))
        {
            //释放用户结点
            pmsg->action = 10;
        }
        else if((0 == strcmp(pmsg->message,"quite")))
        {
            return;
        }
        else
        {
            memset(pmsg->toname, 0, sizeof(pmsg->toname));
            strcpy(pmsg->toname, "all");
            pmsg->action = 4;
            if(write(sockfd,pmsg,sizeof(Msg)) < 0)
            {
                perror("write error!");
                exit(1);
            }
        }
    }
}


void client_file(int port)
{
    int ret;
    int sockfd;
    struct sockaddr_in s_addr;

    sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd < 0)
    {
        perror("socket error!");
        exit(1);
    }

    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(port);
    s_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    ret = connect(sockfd, (struct sockaddr *)&s_addr, sizeof(struct sockaddr_in));
    if(ret < 0)
    {
        perror("connect error!");
        exit(1);
    }

}

Msg *new_fb_head(Msg *p_msg)
{
    Msg*pmsg = (Msg*)malloc(sizeof(Msg));
    memset(pmsg, 0, sizeof(Msg));
    strcpy(pmsg->filename, p_msg->filename);
    strcpy(pmsg->name, p_msg->name);
    strcpy(pmsg->toname, p_msg->toname);
    pmsg->sfile_id = p_msg->sfile_id;
    pmsg->offset = p_msg->offset;
    pmsg->bs = BLOCKSIZE;
    p_msg->offset += BLOCKSIZE;
    return pmsg; 
}

void *send_filedata(void *arg)
{
    Msg*omsg = (Msg*)arg;
    //线程独立
    pthread_t id = pthread_self();
    pthread_detach(id);
    
    int sockfd2 = Create_sock();
    omsg->action = 5;
    //让服务器线程准备好
    write(sockfd2, omsg, sizeof(Msg));

    //连续的映射数据
    int i=0;
    int num = omsg->bs/SEND_SIZE;
    char *fp = mbegin + omsg->offset;
    for(i=0; i<num; i++)
    {
        if((write(sockfd2, fp, SEND_SIZE)) == SEND_SIZE)
        {
            fp += SEND_SIZE;
            omsg->bs = omsg->bs - SEND_SIZE;
        }
    }
    write(sockfd2, fp, omsg->bs);
    close(sockfd2);
    free(arg);
    return NULL;
}

#if 1
void send_file(int sockfd, Msg *pmsg)
{
    int last_bs = 0;
    //得到文件大小stat
    scanf("%s",pmsg->filename);
    int fd = open(pmsg->filename, O_RDWR);
    if(fd < 0)
    {
        printf("文件打开错误！\n");
        exit(1);
    }
    struct stat mystat;
    fstat(fd, &mystat);
    //输入pmsg
    pmsg->action = 3;
    pmsg->filesize = mystat.st_size;

    /*最后一个分块可能不足一个标准分块*/
    int count = pmsg->filesize/BLOCKSIZE;
    if(pmsg->filesize%BLOCKSIZE == 0){
        pmsg->count = count;
    }
    else{
        pmsg->count = count+1;
        last_bs = pmsg->filesize - BLOCKSIZE*count;
    }
    pmsg->bs = BLOCKSIZE;
    
    //发送文件信息
    write(sockfd, pmsg, sizeof(Msg));
    
    //接受数据id；
    while(1)
    {
        pthread_mutex_lock(&mutex);

        pthread_cond_wait(&cond_sfile, &mutex);
        pmsg->sfile_id = sfile_id;

        /*map，关闭fd*/
        mbegin = (char *)mmap(NULL, pmsg->filesize, 
                            PROT_WRITE|PROT_READ, MAP_SHARED, fd , 0);
        close(fd);
        
        //创建线程并发送
        int i_count = 0;
        pmsg->offset = 0;
        pthread_t pid[pmsg->count];
        memset(pid, 0, sizeof(pthread_t)*pmsg->count);
        //文件刚好分块
        if(last_bs == 0)
        {
            for(i_count=0; i_count<pmsg->count; i_count++)
            {
                Msg * p_msg = new_fb_head(pmsg);
                if(pthread_create(&pid[i_count], NULL, send_filedata, (void*)p_msg) != 0)
                {
                    printf("%s:pthread_create failed, errno:%d, error:%s\n", __FUNCTION__, errno, strerror(errno));
                    exit(1);
                }
            }
        }
        //不能被标准分块
        else
        {
            for(i_count=0; i_count<pmsg->count-1; i_count++)
            {
                Msg * p_msg = new_fb_head(pmsg);
                if(pthread_create(&pid[i_count], NULL, send_filedata, (void*)p_msg) != 0)
                {
                    printf("%s:pthread_create failed, errno:%d, error:%s\n", __FUNCTION__, errno, strerror(errno));
                    exit(1);
                }
            }
            //最后一块
            Msg*p_msg = (Msg*)malloc(sizeof(Msg));
            memset(p_msg, 0, sizeof(Msg));
            strcpy(p_msg->filename, pmsg->filename);
            strcpy(p_msg->name, pmsg->name);
            strcpy(p_msg->toname, pmsg->toname);
            p_msg->sfile_id = pmsg->sfile_id;
            p_msg->offset = pmsg->offset;
            p_msg->bs = last_bs;
            if(pthread_create(&pid[i_count], NULL, send_filedata, (void*)p_msg) != 0)
                {
                    printf("%s:pthread_create failed, errno:%d, error:%s\n", __FUNCTION__, errno, strerror(errno));
                    exit(1);
                }
        }

        //回收线程
        for(i_count=0; i_count<pmsg->count; i_count++)
        {
            pthread_join(pid[i_count],NULL);
        }
        break;
    }
    pthread_mutex_unlock(&mutex);
    return;
}

#endif

void log_manager(int sockfd, Msg *pmsg)
{
    memset(pmsg, 0, sizeof(Msg));
    pmsg->action = 7;
    printf("input name\n");
    scanf("%s",pmsg->name);

    printf("input password\n");
    scanf("%s",pmsg->password);    

    if(write(sockfd, pmsg, sizeof(Msg)) < 0)
    {
        perror("write error!");
        exit(1);
    }
    return;
}