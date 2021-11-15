#include "sqlconnpool.h"
using namespace std;
// 构造函数初始化变量
SqlConnPool::SqlConnPool() {
    useCount_ = 0;
    freeCount_ = 0;
}

// 新建一个内存连接池对象，返回其地址
SqlConnPool* SqlConnPool::Instance() {
    static SqlConnPool connPool;
    return &connPool;
}

void SqlConnPool::Init(const char* host, int port,
            const char* user,const char* pwd, const char* dbName,
            int connSize = 10) {
    assert(connSize > 0);

    // 创建connSize个连接
    for (int i = 0; i < connSize; i++) {
        MYSQL *sql = nullptr;
        // 获取sql连接对象，mysql_init为库函数
        sql = mysql_init(sql);
        if (!sql) {
            LOG_ERROR("MySql init error!");
            assert(sql);
        }
        // 连接数据库，mysql_real_connect为库函数
        sql = mysql_real_connect(sql, host,
                                 user, pwd,
                                 dbName, port, nullptr, 0);
        if (!sql) {
            LOG_ERROR("MySql Connect error!");
        }
        // 加入连接队列
        connQue_.push(sql);
    }
    MAX_CONN_ = connSize;
    // 初始化信号量, 信号量初始值为MAX_CONN_，表示空闲的连接数
    // 用sem_wait()函数获取信号量，当信号量大于0时，执行操作，然后将信号量减1
    // 若信号量等于0，则阻塞等待
    // 释放连接是post操作，信号量加1
    sem_init(&semId_, 0, MAX_CONN_);
}

MYSQL* SqlConnPool::GetConn() {
    MYSQL *sql = nullptr;
    if(connQue_.empty()){
        LOG_WARN("SqlConnPool busy!");
        return nullptr;
    }
    sem_wait(&semId_);
    {
        // lock_guard<mutex>用于跳出函数时自动释放锁，不用手动释放
        lock_guard<mutex> locker(mtx_);
        sql = connQue_.front();
        connQue_.pop();
    }
    return sql;
}

void SqlConnPool::FreeConn(MYSQL* sql) {
    assert(sql);
    lock_guard<mutex> locker(mtx_);
    connQue_.push(sql);
    sem_post(&semId_);
}

void SqlConnPool::ClosePool() {
    // 关闭池子时，将连接队列所有的sql连接对象都移出来逐一关闭
    lock_guard<mutex> locker(mtx_);
    while(!connQue_.empty()) {
        auto item = connQue_.front();
        connQue_.pop();
        mysql_close(item);
    }
    // 整体关闭
    mysql_library_end();
}

int SqlConnPool::GetFreeConnCount() {
    // 返回可用的sql连接对象
    lock_guard<mutex> locker(mtx_);
    return connQue_.size();
}

SqlConnPool::~SqlConnPool() {
    ClosePool();
}
