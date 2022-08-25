#include "pthreadpool.h"
#include "server_w.h"

int main(int argc, char *argv[])
{
    LOG_SetPrintDebugLogFlag(1);//打印调试信息
	LOG_SetPrintLogPlaceFlag(1);//保存打印信息到文件
    LOG_Init("info", 8000);     //创建并初始化日志文件
    //打开一个数据库
    createlist();
    //创建线程池
    ThreadPool* pool = threadPoolCreate(3, 10, 100);
    //设置信号,忽视SIGPIPE，防止SIGPIPE带来的进程关闭
    signal(SIGPIPE,SIG_IGN);
    
    int sockfd = Create_sock();
    //创建一个文件描述符，一个事件中间变量，一个epoll队列
    int epfd;
    struct epoll_event event;
    struct epoll_event events[EVENTS_SIEZ];
    //返回句柄，监听数多大
    epfd = epoll_create(EVENTS_SIEZ);
    //向事件合集中添加fd
    event.events = EPOLLIN;
    event.data.fd = sockfd;
    //文件描述符，事件：向事件队列中添加，sockfd
    epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &event);

    socklen_t c_size;
    struct sockaddr_in c_addr;//接收结构体
    c_size = sizeof(struct sockaddr_in);
    while(1)
    {
        //等待事件的到来,返回有几个事件，形参：句柄，事件队列，队列长度，定时（超时返回零）
        int count = epoll_wait(epfd, events, EVENTS_SIEZ, -1);
        for(int i=0; i<count; i++)
        {
            if(events[i].events == EPOLLIN)
            {
                if(events[i].data.fd == sockfd)
                {
                    //接受建立连接，返回已建立连接套接字，形参：接受连接请求的fd，接受结构体变量，接受结构体长度
                    long cfd = accept(sockfd,(struct sockaddr*)&c_addr,&c_size);
                    if(cfd < 0)
                    {
                        perror("accept error!");
                        exit(1);
                    }
                    printf("ip = %s port = %d\n",inet_ntoa(c_addr.sin_addr),ntohs(c_addr.sin_port));
                    pthread_t id;
                    // 向线程池添加任务
                    threadPoolAdd(pool, thread_read, (void*)cfd);
                }
            }
        }
    }
}