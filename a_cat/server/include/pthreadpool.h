#ifndef _THREADPOOL_H
#define _THREADPOOL_H

#include "log.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define NUMBERE 2

//任务结构体
typedef struct Task
{
    void* (*function)(void*arg);  //回调函数入口地址
    void *arg;                   //传输对象的fd
}Task;

// 线程池结构体
struct ThreadPool
{
    // 任务队列
    Task* taskQ;
    int queueCapacity;  // 容量
    int queueSize;      // 当前任务个数
    int queueFront;     // 队头 -> 取数据
    int queueRear;      // 队尾 -> 放数据

    pthread_t managerID;    // 管理者线程ID
    pthread_t *threadIDs;   // 工作的线程ID
    int minNum;             // 最小线程数量
    int maxNum;             // 最大线程数量
    int busyNum;            // 忙的线程的个数
    int liveNum;            // 存活的线程的个数
    int exitNum;            // 要销毁的线程个数
    pthread_mutex_t mutexPool;  // 锁整个的线程池
    pthread_mutex_t mutexBusy;  // 锁busyNum变量
    pthread_cond_t notFull;     // 任务队列是不是满了
    pthread_cond_t notEmpty;    // 任务队列是不是空了

    int shutdown;           // 是不是要销毁线程池, 销毁为1, 不销毁为0
};


typedef struct ThreadPool ThreadPool;
// 创建线程池并初始化
ThreadPool *threadPoolCreate(int min, int max, int queueSize);

// 销毁线程池
int threadPoolDestroy(ThreadPool* pool);

// 给线程池添加任务
void threadPoolAdd(ThreadPool* pool, void*(*func)(void*), void* arg);

// 获取线程池中工作的线程的个数
int threadPoolBusyNum(ThreadPool* pool);

// 获取线程池中活着的线程的个数
int threadPoolAliveNum(ThreadPool* pool);

//////////////////////
// 工作的线程(消费者线程)任务函数
void* worker(void* arg);
// 管理者线程任务函数
void* manager(void* arg);
// 单个线程退出
void threadExit(ThreadPool* pool);
#endif  // _THREADPOOL_H