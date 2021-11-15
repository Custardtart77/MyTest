#ifndef SQLCONNPOOL_H
#define SQLCONNPOOL_H

#include <mysql/mysql.h>
#include <string>
#include <queue>
#include <mutex>
#include <semaphore.h>
#include <thread>
#include "../log/log.h"

class SqlConnPool {
public:
    // 静态函数，不用实例化类也能够调用
    // 单例模式
    static SqlConnPool *Instance();
    // 获取连接
    MYSQL *GetConn();

    // 释放连接，放回池子里面
    void FreeConn(MYSQL * conn);

    // 获取空闲的数量
    int GetFreeConnCount();
    /* 主机名，端口自，用户，密码， 数据库名，连接数量*/
    void Init(const char* host, int port,
              const char* user,const char* pwd,
              const char* dbName, int connSize);
    void ClosePool();

private:
    SqlConnPool();
    ~SqlConnPool();

    int MAX_CONN_;  // 最大的连接数
    int useCount_;  // 当前的用户数
    int freeCount_; // 空闲的用户数

    std::queue<MYSQL *> connQue_;   // 队列（MYSQL *） MYSQL *用于操作数据库的
    std::mutex mtx_;    // 互斥锁
    sem_t semId_;   // 信号量
};


#endif // SQLCONNPOOL_H
