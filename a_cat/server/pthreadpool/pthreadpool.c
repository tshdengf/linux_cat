#include "pthreadpool.h"
/*******************************************
 ** 函数名：threadPoolCreate
 ** 输  入：min 最小线程数
           max 最大线程数
           queueSize 任务队列的总数
 ** 输  出：ThreadPool* pool 线程池结构体
 ** 功能描述：创建线程池并初始化
 ** 返回值类型：ThreadPool*
 *******************************************/
ThreadPool* threadPoolCreate(int min, int max, int queueSize)
{
    ThreadPool* pool = (ThreadPool*)malloc(sizeof(ThreadPool));
    do 
    {
        if (pool == NULL)
        {
            printf("malloc threadpool fail...\n");
            break;
        }

        pool->threadIDs = (pthread_t*)malloc(sizeof(pthread_t) * max);
        if (pool->threadIDs == NULL)
        {
            printf("malloc threadIDs fail...\n");
            break;
        }
        memset(pool->threadIDs, 0, sizeof(pthread_t) * max);
        pool->minNum = min;
        pool->maxNum = max;
        pool->busyNum = 0;
        pool->liveNum = min;    // 和最小个数相等
        pool->exitNum = 0;

        if (pthread_mutex_init(&pool->mutexPool, NULL) != 0 ||
            pthread_mutex_init(&pool->mutexBusy, NULL) != 0 ||
            pthread_cond_init(&pool->notEmpty, NULL) != 0 ||
            pthread_cond_init(&pool->notFull, NULL) != 0)
        {
            printf("mutex or condition init fail...\n");
            break;
        }

        // 任务队列
        pool->taskQ = (Task*)malloc(sizeof(Task) * queueSize);
        pool->queueCapacity = queueSize;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;

        pool->shutdown = 0;

    	LOG_INFO(LOG_DEBUG, "%s", "创建线程");
        // 创建线程
        pthread_create(&pool->managerID, NULL, manager, pool);
        for (int i = 0; i < min; ++i)
        {
            pthread_create(&pool->threadIDs[i], NULL, worker, pool);
        }
        return pool;
    } while (0);

    // 释放资源
    if (pool && pool->threadIDs) free(pool->threadIDs);
    if (pool && pool->taskQ) free(pool->taskQ);
    if (pool) free(pool);

    return NULL;
}

/*******************************************
 ** 函数名：threadPoolDestroy
 ** 输  入：ThreadPool* pool 线程池结构体指针
 ** 输  出： 0
 ** 功能描述：销毁线程池，并回收资源
 ** 返回值类型：int
 *******************************************/
int threadPoolDestroy(ThreadPool* pool)
{
    if (pool == NULL)
    {
        return -1;
    }

    // 关闭线程池
    pool->shutdown = 1;
    // 阻塞回收管理者线程
    pthread_join(pool->managerID, NULL);
    // 唤醒阻塞的消费者线程,让其自我销毁
    for (int i = 0; i < pool->liveNum; ++i)
    {
        pthread_cond_signal(&pool->notEmpty);
    }
    // 释放堆内存
    if (pool->taskQ)
    {
        free(pool->taskQ);
    }
    if (pool->threadIDs)
    {
        free(pool->threadIDs);
    }

    pthread_mutex_destroy(&pool->mutexPool);   //销毁锁
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_cond_destroy(&pool->notEmpty);     //销毁条件变量
    pthread_cond_destroy(&pool->notFull);

    free(pool);
    pool = NULL;

    return 0;
}

/*******************************************
 ** 函数名：threadPoolAdd
 ** 输  入：ThreadPool* pool 线程池结构体指针
           void(*func)(void*) 需要执行的函数入口地址 
           void* arg 需要执行的函数的传参
 ** 输  出： void
 ** 功能描述：给线程池的任务队列添加任务
 ** 返回值类型：void
 *******************************************/
void threadPoolAdd(ThreadPool* pool, void*(*func)(void*), void* arg)
{
    pthread_mutex_lock(&pool->mutexPool);
    //任务队列满了，循环等待消费队列发来任务队列不为满的条件变量
    while (pool->queueSize == pool->queueCapacity && !pool->shutdown)
    {
        // 任务队列不为空，则阻塞生产者线程，释放线程池的锁mutexPool
        pthread_cond_wait(&pool->notFull, &pool->mutexPool);
    }
    //如果需要销毁线程池则释放锁并不再添加任务
    if (pool->shutdown)
    {
        pthread_mutex_unlock(&pool->mutexPool);
        return;
    }
    // 添加任务
    pool->taskQ[pool->queueRear].function = func;
    pool->taskQ[pool->queueRear].arg = arg;
    //循环队列
    pool->queueRear = (pool->queueRear + 1) % pool->queueCapacity;
    pool->queueSize++;
    //告诉消费线程任务队列不为空
    pthread_cond_signal(&pool->notEmpty);
    pthread_mutex_unlock(&pool->mutexPool);
}

/*******************************************
 ** 函数名：threadPoolBusyNum
 ** 输  入：ThreadPool* pool 线程池结构体指针
 ** 输  出： int busyNum
 ** 功能描述：返回忙的线程数量
 ** 返回值类型：int
 *******************************************/
int threadPoolBusyNum(ThreadPool* pool)
{
    pthread_mutex_lock(&pool->mutexBusy);
    int busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busyNum;
}

/*******************************************
 ** 函数名：threadPoolAliveNum
 ** 输  入：ThreadPool* pool 线程池结构体指针
 ** 输  出： int aliveNum
 ** 功能描述：返回存活的线程数量
 ** 返回值类型：int
 *******************************************/
int threadPoolAliveNum(ThreadPool* pool)
{
    pthread_mutex_lock(&pool->mutexPool);
    int aliveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);
    return aliveNum;
}

/*******************************************
 ** 函数名：worker
 ** 输  入：void* arg是一个线程池结构体指针
 ** 输  出： NULL
 ** 功能描述：将任务从任务队列中取出并执行
 ** 返回值类型：void *
 *******************************************/
void* worker(void* arg)
{
    ThreadPool* pool = (ThreadPool*)arg;

    while (1)
    {
        pthread_mutex_lock(&pool->mutexPool);
        // 当前任务队列是否为空，循环等待添加队列发来任务队列为满的条件变量
        while (pool->queueSize == 0 && !pool->shutdown)
        {
            // 阻塞工作线程,等待生成线程告诉任务队列不为空的条件变量
            pthread_cond_wait(&pool->notEmpty, &pool->mutexPool);

            // 判断是不是要销毁线程
            if (pool->exitNum > 0)
            {
                pool->exitNum--;
                if (pool->liveNum > pool->minNum)
                {
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    threadExit(pool);
                }
            }
        }

        // 判断线程池是否被关闭了
        if (pool->shutdown)
        {
            pthread_mutex_unlock(&pool->mutexPool);
            threadExit(pool);
        }

        // 从任务队列中取出一个任务
        Task task;
        task.function = pool->taskQ[pool->queueFront].function;
        task.arg = pool->taskQ[pool->queueFront].arg;
        // 移动头结点
        pool->queueFront = (pool->queueFront + 1) % pool->queueCapacity;
        pool->queueSize--;
        // 解锁, 告诉生成线程不为满
        pthread_cond_signal(&pool->notFull);
        pthread_mutex_unlock(&pool->mutexPool);

        printf("thread %ld start working...\n", pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;
        pthread_mutex_unlock(&pool->mutexBusy);
        //执行程序
        task.function(task.arg);
        free(task.arg);
        task.arg = NULL;
        //结束后减减忙线程
        printf("thread %ld end working...\n", pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexBusy);
    }
    return NULL;
}
/*******************************************
 ** 函数名：manager
 ** 输  入：void* arg是一个线程池结构体指针
 ** 输  出： NULL
 ** 功能描述：定时检测，进行添加线程与删除线程
 ** 返回值类型：void *
 *******************************************/
void* manager(void* arg)
{
    ThreadPool* pool = (ThreadPool*)arg;
    while (!pool->shutdown)
    {
        // 每隔3s检测一次
        sleep(3);

        // 取出线程池中任务的数量和当前线程的数量
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->queueSize;
        int liveNum = pool->liveNum;
        pthread_mutex_unlock(&pool->mutexPool);

        // 取出忙的线程的数量
        pthread_mutex_lock(&pool->mutexBusy);
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexBusy);

        // 添加线程
        // 任务的个数>存活的线程个数 && 存活的线程数<最大线程数
        if (queueSize > liveNum && liveNum < pool->maxNum)
        {
            pthread_mutex_lock(&pool->mutexPool);
            int counter = 0;
            for (int i = 0; i < pool->maxNum && counter < NUMBERE
                && pool->liveNum < pool->maxNum; ++i)
            {
                if (pool->threadIDs[i] == 0)
                {
                    pthread_create(&pool->threadIDs[i], NULL, worker, pool);
                    counter++;
                    pool->liveNum++;
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);
        }
        // 销毁线程
        // 忙的线程*2 < 存活的线程数 && 存活的线程>最小线程数
        if (busyNum * 2 < liveNum && liveNum > pool->minNum)
        {
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = NUMBERE;
            pthread_mutex_unlock(&pool->mutexPool);
            // 让工作的线程自杀
            for (int i = 0; i < NUMBERE; ++i)
            {
                pthread_cond_signal(&pool->notEmpty);
            }
        }
    }
    return NULL;
}

/*******************************************
 ** 函数名：threadExit
 ** 输  入：ThreadPool* pool是一个线程池结构体指针
 ** 输  出： NULL
 ** 功能描述：在线程组中遍历，找到当前线程销毁
 ** 返回值类型：void 
 *******************************************/
void threadExit(ThreadPool* pool)
{
    //识别当前线程号
    pthread_t tid = pthread_self();
    for (int i = 0; i < pool->maxNum; ++i)
    {
        if (pool->threadIDs[i] == tid)
        {
            pool->threadIDs[i] = 0;
            printf("threadExit() called, %ld exiting...\n", tid);
            break;
        }
    }
    pthread_exit(NULL);
}