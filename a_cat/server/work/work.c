#include "pthreadpool.h"
#include "server_w.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
struct online *head = NULL;
char *errmsg;
sqlite3 *db;
int addr[100] = {0};


int my_strcmp(char *dest,char *src)
{
    printf("dest:%s\n",dest);
    printf("src:%s\n",src);
    int i = 0;
    while(dest[i] == src[i])
    {
        i++;
        if(dest[i] == '\0')
        {
            return 1;
        }
    }
    return 0;
}

/*******************************************
 ** 函数名：createlist
 ** 输  入：void
 ** 输  出：void
 ** 功能描述：创建数据库并初始化列表
 ** 返回值类型：void
 *******************************************/
void createlist()
{
    char sql[1024] = {0};
    int ret = sqlite3_open("test.db",&db);

	LOG_INFO(LOG_DEBUG, "%s", "create table if not exists person");
    
    memset(sql,0,sizeof(sql));
    strcpy(sql,"create table if not exists person(id integer primary key,name text,password text);");
    ret = sqlite3_exec(db,sql,NULL,NULL,&errmsg);
    
    memset(sql,0,sizeof(sql));
    strcpy(sql,"create table if not exists manager(id integer primary key,name text,password text);");
    ret = sqlite3_exec(db,sql,NULL,NULL,&errmsg);
    
    memset(sql,0,sizeof(sql));
    strcpy(sql,"create table if not exists storagemessage(fromname text, toname text,message text, timestamp not NULL default(datetime('now','localtime')));");
    ret = sqlite3_exec(db,sql,NULL,NULL,&errmsg);
    return;
}

void insert_link(struct online *new_user)
{
    new_user->next = head;
    head = new_user;
}

int find_fd(char *toname)
{
    struct online*temp = head;
    while(temp != NULL)
    {
        if(strcmp(temp->name,toname) == 0)
        {
            return temp->cfd;
        }
        temp = temp->next;
    }
    return -1;
}
//形参：para
int my_sqlite_callback(void *para,int contun,char **columvalue,char **columnName)
{
    strcpy((char *)para,columvalue[0]);

    return 0;
}
int querypassword(Msg *msg)
{
	LOG_INFO(LOG_DEBUG, "%s", "querypassword!!");
    char sql[1024] = {0};
    char password[20];
    sqlite3_open("test.db",&db);

    sprintf(sql,"select password from person where name = '%s';",msg->name);
    sqlite3_exec(db,sql,my_sqlite_callback,(void*)password,&errmsg);
    if(my_strcmp(password,msg->password) == 1)
    {
        return 1;
    }
	LOG_INFO(LOG_DEBUG, "%s", "querypassword!!---out");
    return 0;
}

/*******************************************
 ** 函数名：thread_read
 ** 输  入：void *arg  接受对象的套接字
 ** 输  出：int
 ** 功能描述：读出接受结构体的名字
 ** 返回值类型：int
 *******************************************/
int queryname(Msg *msg)
{
	LOG_INFO(LOG_DEBUG, "%s", "queryname!!");
    char sql[1024] = {0};
    char name[20];
    sqlite3_open("test.db",&db);

    sprintf(sql,"select name from person where name = '%s';",msg->name);
    sqlite3_exec(db,sql,my_sqlite_callback,(void*)name,&errmsg);
    if(my_strcmp(name,msg->name) == 1)
    {
        return 1;
    }
	LOG_INFO(LOG_DEBUG, "%s", "queryname!!---out");
    return 0;
}

void get_time(char *now_time)
{
    time_t now;
    struct tm *tm_now;
    time(&now);
    tm_now = localtime(&now);
    memset(now_time, 0, sizeof(now_time));
    // sprintf(now_time, "%d-%d-%d %d:%d:%d", tm_now->tm_year + 1900, tm_now->tm_mon + 1, 
    //                     tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
        sprintf(now_time, "%d-%d-%d %d:%d:%d", tm_now->tm_year + 1900, tm_now->tm_mon + 1, 
                        tm_now->tm_mday, tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec);
}

void insert_data_message(Msg *pmsg)
{
    char sql[2048] = {0};
    char now_time[32] = {0};
    get_time(now_time);
    memset(sql, 0, sizeof(sql));
    sprintf(sql, "insert into storagemessage(fromname,toname,message) values('%s', '%s', '%s');",
                pmsg->name, pmsg->toname, pmsg->message);
    sqlite3_exec(db, sql, NULL, NULL, &errmsg);
    return;
}

int my_getmessage_callback(void *para, int contun, char **columvalue, char **columnName)
{
    // printf("进入回调\n");
    struct callback* call = (struct callback*)para;
    // printf("call name = %s,toname = %s",call->name,call->toname);
    if(strcmp(call->name, columvalue[0]) == 0)
    {
        fprintf(call->fp,"%s:我，时间:%s\n",columvalue[2],columvalue[3]);
        // printf("%s:我，时间:%s\n",columvalue[2],columvalue[3]);
    }
    else if(strcmp(call->toname, columvalue[0]) == 0)
    {
        fprintf(call->fp,"时间:%s,%s:%s\n",columvalue[3],columvalue[0],columvalue[2]);
        // printf("时间:%s,%s:%s\n",columvalue[3],columvalue[0],columvalue[2]);
    }

    return 0;
}

FILE *select_message(Msg *pmsg)
{
    char sql[2048] = {0};
    FILE *fp = fopen("message.text","w+");
    struct callback call;
    call.fp = fp;
    strcpy(call.name, pmsg->name);
    strcpy(call.toname, pmsg->toname);
    printf("开始回调\n");
    sprintf(sql, "select fromname,toname,message,timestamp from storagemessage where (toname = '%s' and fromname = '%s') or (fromname = '%s' and toname = '%s') order by timestamp desc limit 8;",pmsg->toname,pmsg->name,pmsg->toname,pmsg->name);
    sqlite3_exec(db, sql, my_getmessage_callback, (void*)&call, &errmsg);
    
    return fp;
}

/*******************************************
 ** 函数名：Create_sock
 ** 输  入：void
 ** 输  出：int sockfd
 ** 功能描述：返回被动等待监听连接的套接字
 ** 返回值类型：int
 *******************************************/
int Create_sock()
{
    int ret;
    int sockfd;
    struct sockaddr_in s_addr;
/*基于文件： AF_UNIX  
  基于网络： AF_INET
  面向连接： SOCK_STREAM
  面向无连接:SOCK_DGRAM   0指不指定协议模式*/
    sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd < 0)
    {
        perror("socket error!");
        exit(1);
    }

    int opt = 1;
    /*1.套接字描述符
      2.设置级别为套接字级别
      3.实现端口复用功能
      4.设置指向缓存区大小，默认为8k，设置小避免不必要的开销  */
    setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));//设置套接字可以重复使用端口号(TIME_WAIT)

    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(5556);//将主机短整型转为网络字节,(x86下小端序转为网络大端序)
    s_addr.sin_addr.s_addr = inet_addr("127.0.0.1");//将一个字符串格式的ip地址转换成一个uint32_t数字格式。
    //s_addr按照网络字节顺序存储IP地址 sockaddr_in和sockaddr是并列的结构.
    ret = bind(sockfd,(struct sockaddr *)&s_addr,sizeof(struct sockaddr_in));
    if(ret < 0)
    {
        perror("bind error!");
        exit(1);
    }
    //将套接字描述符变为被动描述符，用于被动监听客户连接
    ret = listen(sockfd,2000);
    if(ret < 0)
    {
        perror("bind error!");
        exit(1);
    }

    return sockfd;
}

void createfile(char *filename, char *filesize)
{
    int fd = open(filename, O_RDWR | O_CREAT);
    fchmod(fd, S_IRGRP | S_IRUSR | S_IWUSR | S_IROTH);
    lseek(fd, (*filesize)-1, SEEK_SET);
    write(fd, " ", 1);
    close(fd);
}

void send_file(Msg*pmsg,int cofd,int tofd)
{
    char *fp = (char *)malloc(RECVBUF_SIZE);
    //从cofd读到后，写入tofd
    //接受数据，往map内存写
    int remain_size = pmsg->bs; //数据块中待接收数据大小
    int err = pmsg->bs % RECVBUF_SIZE;
    int size = 0; //一次recv接受数据大小

#if 1
    while (remain_size > 0)
    {
        if ((size = recv(cofd, fp, RECVBUF_SIZE, MSG_WAITALL)) > 0)
        {
            write(tofd, fp, size); 
            remain_size -= size;
        }
        if (remain_size <= err)
        {
            size = recv(cofd, fp, remain_size, 0);
            write(tofd, fp, size); 
            remain_size -= size;
        }
    }
    free(fp);
    printf("转发中...\n");

    close(tofd);
    close(cofd);
#endif
}
void to_all(Msg *pmsg)
{
    struct online *pNode = head;
    while(pNode != NULL)
    {
        write(pNode->cfd, pmsg, sizeof(Msg));
        pNode = pNode->next;
    }
    return;
}

struct online *get_online(char *name)
{
    struct online *pNode = head;
    while(pNode != NULL)
    {
        if(strcmp(pNode->name, name) == 0)
        {
            return pNode;
        }
        pNode = pNode->next;
    }
    return NULL;
}

/*******************************************
 ** 函数名：thread_read
 ** 输  入：void *arg  接受对象的套接字
 ** 输  出：void *
 ** 功能描述：循环接受处理，读到的数据
 ** 返回值类型：int
 *******************************************/
void* thread_read(void *arg)
{
    pthread_t id = pthread_self();
    /*指定该状态，线程主动与主控线程断开关系。
    线程结束后（不会产生僵尸线程），其退出状态不由其他线程获取，
    而直接自己自动释放（自己清理掉PCB的残留资源）。
    网络、多线程服务器常用。*/
    pthread_detach(id);
    long cfd = (long)arg;

    Msg *pmsg = (Msg *)malloc(sizeof(Msg));
    
    while(1)
    {
        memset(pmsg,0,sizeof(Msg));
        /*从连接的套接字或绑定的无连接套接字接收数据。
        返回值：读出的字节数。没有数据则阻塞等待
        客户端关闭返回0，释放客户端socket.
        形参：从哪里读，到哪里去，读多少。一般为0
        */
        int r_n = recv(cfd,pmsg,sizeof(Msg), 0);
        if(r_n == 0)
        {
            printf("server exit!\n");
            //将线程用户从在线链表中删除

            pthread_exit(NULL);
        }
        switch(pmsg->action)
        {
            case 0:
            {
                //判断用户名是否注册
                if(1 == queryname(pmsg))
                {
                    pmsg->action = 255;
                    write(cfd, pmsg, sizeof(Msg));
                    break;
                }
                char sql[100];
                memset(sql, 0, sizeof(sql));
                
                int ret = sqlite3_open("test.db", &db);
                if(ret < 0)
                {
                    perror("sqlite3_open error!");
                }
                sprintf(sql,"insert into person(name,password)values('%s',%s);",pmsg->name,pmsg->password);
                ret = sqlite3_exec(db, sql, NULL, NULL, &errmsg);

                break;
            }
            case 1:
            {
                if(0 == querypassword(pmsg))
                {
                    pmsg->action = -1;
                    write(cfd,pmsg,sizeof(Msg));
                    break;
                }

                struct online *new_user = (struct online *)malloc(sizeof(struct online));
                strcpy(new_user->name, pmsg->name);
                new_user->cfd = cfd;
                new_user->say = 0;
                insert_link(new_user);
                
                write(cfd,pmsg,sizeof(Msg));
                break;
            }
            case 2:
            {
                insert_data_message(pmsg);
                int to_fd = find_fd(pmsg->toname);
                if(to_fd == -1)
                {
                    pmsg->action = -2;
                    write(cfd, pmsg, sizeof(Msg));
                }
                else
                {
                    write(to_fd, pmsg, sizeof(Msg));
                }
                break;
            }
            case 3:
            {
                int to_fd = find_fd(pmsg->toname);
                if(to_fd == -1)
                {
                    pmsg->action = -2;
                    write(cfd, pmsg, sizeof(Msg));
                }
                else
                {
                    int i = 0;
                    while(addr[i] == 1)
                    {
                        i++;
                    }
                    addr[i] = 1;
                    pmsg->sfile_id = i;
                    write(cfd, pmsg, sizeof(Msg));
                    pmsg->action = -3;
                    write(to_fd, pmsg, sizeof(Msg));
                }
                break;
            }
            case 4:
            {
                insert_data_message(pmsg);
                struct online *pNode = head;
                while(pNode != NULL)
                {
                    if(strcmp(pNode->name, pmsg->name) == 0)
                    {
                        if(pNode->say != 1)
                        {
                            to_all(pmsg);
                            break;
                        }
                        else if(pNode->say == 1)
                        {
                            pmsg->action = 7;
                            write(cfd, pmsg, sizeof(Msg));
                            break;
                        }
                    }

                }
                break;
            }
            case 5:
            {
                //要传输对象的套接字
                int sofd = 0;
                //找到要传输的对象
                int to_fd = find_fd(pmsg->toname);
                if(to_fd == -1)
                {
                    pmsg->action = -2;
                    write(cfd, pmsg, sizeof(Msg));
                }  
                else
                {
                    //通知接收客户端做好准备；
                    write(to_fd, pmsg, sizeof(Msg));
                    //循环找cfd,用循环队列或者循环链表储存cfd。
                    for(int i=0; i<10; i = (++i)%10)
                    {
                        pthread_mutex_lock(&mutex);
                        if(find_cfd[i].use == 1)
                        {
                            if(find_cfd[i].offset == pmsg->offset)
                            {
                                sofd = find_cfd[i].cfd;
                                find_cfd[i].use = 0;
                                pthread_mutex_unlock(&mutex);
                                break;
                            }
                            else
                            {
                            }
                        }
                        pthread_mutex_unlock(&mutex);
                    }                    

                    send_file(pmsg,cfd,sofd);
                    pthread_cancel(id);
                }
                break;
            }
            case -5:
            {
                for(int i=0; i<10; i = (++i)%10)
                {
                    pthread_mutex_lock(&mutex);
                    if(find_cfd[i].use == 0)
                    {
                        strcpy(find_cfd[i].filename, pmsg->filename);
                        find_cfd[i].cfd = cfd;
                        find_cfd[i].use = 1;
                        find_cfd[i].offset = pmsg->offset;

                        
                        pthread_mutex_unlock(&mutex);
                        break;
                    }
                    pthread_mutex_unlock(&mutex);

                }
                break;
            }
            case 6:
            {
                char buf[1024];
                int ret = -1;
                memset(buf, 0, sizeof(buf));
                FILE *fp = fopen("user.txt", "w+");
                struct online*pNode = head;
                while(1)
                {
                    if(pNode->next == NULL)
                    {
                        fprintf(fp, "用户名:%s", pNode->name);
                        break;
                    }
                    fprintf(fp, "用户名:%s\n", pNode->name);
                    memset(pmsg->message, 0, sizeof(pmsg->message));
                    pNode = pNode->next;
                }
                fseek(fp, 0, SEEK_SET);
                ret = fread(buf, sizeof(buf), 1, fp);
                {
                    memcpy(pmsg->message, buf, strlen(buf));
                    pNode = head;
                    while(pNode != NULL)
                    {
                        write(pNode->cfd, pmsg, sizeof(Msg));
                        pNode = pNode->next;
                    }
                    memset(pmsg->message, 0, sizeof(pmsg->message));
                    memset(buf, 0, sizeof(buf));
                }
                printf("ret = %d\n",ret);
                fclose(fp);
                break;
            }
            case 7:
            {
                if(0 == query_manager_password(pmsg))
                {
                    pmsg->action = -1;
                }

                struct online *new_user = (struct online *)malloc(sizeof(struct online));
                strcpy(new_user->name, pmsg->name);
                new_user->cfd = cfd;
                new_user->say = 0;
                insert_link(new_user);
                pmsg->action = 1;
                write(cfd,pmsg,sizeof(Msg));
                break;
            }
            case 8:
            {
                struct online *pNode = get_online(pmsg->toname);
                if(pNode == NULL)
                {
                    pmsg->action = -2;
                    write(cfd, pmsg, sizeof(Msg));
                    break;
                }
                pNode->say = 1;
                write(cfd, pmsg, sizeof(Msg));
                break;
            }
            case -8:
            {
                struct online *pNode = get_online(pmsg->name);
                if(pNode == NULL)
                {
                    pmsg->action = -2;
                    write(cfd, pmsg, sizeof(Msg));
                    break;
                }
                pNode->say = 0;
                write(cfd, pmsg, sizeof(Msg));
                break;
            }
            case 9:
            {
                char buf[1024];
                int ret = -1;
                memset(buf, 0, sizeof(buf));
                FILE *fp = select_message(pmsg);

                fseek(fp, 0, SEEK_SET);
                while(feof(fp) == 0)
                {
                    fread(buf, sizeof(buf), 1, fp);
                    memcpy(pmsg->message, buf, strlen(buf));
                    
                    write(cfd, pmsg, sizeof(Msg));
                    memset(pmsg->message, 0, sizeof(pmsg->message));
                    memset(buf, 0, sizeof(buf));
                }
                fclose(fp);
                break; 
            }
            case 10:
            {
                struct online *pNode = head;
                struct online *temp = NULL;
                while(pNode != NULL)
                {
                    if(strcmp(pNode->name, pmsg->name) == 0)
                    {
                        if(temp != NULL)
                        {
                            temp->next = pNode->next;
                        }
                        else
                        {
                            head = pNode->next;
                        }
                        if(pNode != NULL)
                        {
                            free(pNode);
                            pNode = NULL;
                        }
                        break;
                    }
                    temp = pNode;
                    pNode = pNode->next;
                }  
                break;              
            }
        }

    }
}

int query_manager_password(Msg *msg)
{
    char sql[1024] = {0};
    char password[20];

    sprintf(sql,"select password from manager where name = '%s';",msg->name);
    sqlite3_exec(db,sql,my_sqlite_callback,(void*)password,&errmsg);
    if(my_strcmp(password,msg->password) == 1)
    {
        return 1;
    }
    return 0;
}

//先写在函数内，调试好以后用execl调用
void server_file()//端口号先写死5858.                
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

    int opt = 1;
    setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));//设置套接字可以重复使用端口号

    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(5858);
    s_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    ret = bind(sockfd,(struct sockaddr *)&s_addr,sizeof(struct sockaddr_in));
    if(ret < 0)
    {
        perror("bind error!");
        exit(1);
    }

    ret = listen(sockfd,20);
    if(ret < 0)
    {
        perror("bind error!");
        exit(1);
    }

    socklen_t c_size;
    struct sockaddr_in c_addr;
    c_size = sizeof(struct sockaddr_in);

    while(1)
    {
        int cfd = accept(sockfd,(struct sockaddr*)&c_addr,&c_size);
        if(cfd < 0) 
        {
            perror("accept error!");
            exit(1);
        }
        printf("ip = %s port = %d\n",inet_ntoa(c_addr.sin_addr),ntohs(c_addr.sin_port));

    }
}