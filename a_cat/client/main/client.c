 #include "work.h"

int lognum = 0;

int main(int argc, char *argv[])
{
    pthread_t id;
    long sockfd = Create_sock();
    //创建一个线程来接收服务器发来的消息并处理
    pthread_create(&id,NULL,thread_read,(void *)sockfd);

    int num;
    Msg *pmsg = (Msg *)malloc(sizeof(Msg));    //创建一个结构体
    
    while(1)
    {
        lognum = 0;
        viewlogin();
        scanf("%d",&num);
        if( 0 > num || num > 3)
        {
            continue;
        }
        if(num == 0)
        {
            exit(1);
        }
        if(num == 2)//进入注册函数
        {
            reg_user(sockfd,pmsg);
        }
        if(num == 1)
        {
            log_user(sockfd, pmsg);
            while(1)
            {
                if(3 == lognum)
                {
                    break;
                }
                if(-1 == lognum)
                {
                    printf("登录失败\n");
                    sleep(1);
                    break;
                }
                else if(1 == lognum)
                {
                    view_user(sockfd, pmsg);
                    while(1)
                    {
                        viewhome(sockfd, pmsg);
                        scanf("%d",&lognum);
                        if(1 == lognum)
                        {
                            chat_user(sockfd, pmsg);
                            continue;
                        }
                        if(2 == lognum)
                        {
                            all_user(sockfd, pmsg);
                            continue;
                        }
                        if(3 == lognum)
                        {
                            quite_user(sockfd, pmsg);
                            break;
                        }
                    }
                }
            }           
        }
        // if(num == 3)
        // {
        //     log_manager(sockfd, pmsg);
        //     while(1)
        //     {
        //         if(-1 == lognum)
        //         {
        //             printf("登录失败\n");
        //             sleep(1);
        //             break;
        //         }
        //         else if(1 == lognum)
        //         {
        //             while(1)
        //             {
        //                 viewhome(sockfd, pmsg);
        //                 scanf("%d",&lognum);
        //                 if(1 == lognum)
        //                 {
        //                     chat_user(sockfd, pmsg);
        //                     continue;
        //                 }
        //                 if(2 == lognum)
        //                 {
        //                     all_manager(sockfd, pmsg);
        //                     continue;
        //                 }
        //                 if(-1 == lognum)
        //                 {
        //                     break;
        //                 }
        //             }
        //         }
        //     }           
        // }
    }
}