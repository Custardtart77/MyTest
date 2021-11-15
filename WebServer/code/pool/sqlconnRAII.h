#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H
#include "sqlconnpool.h"

/* 资源在对象构造初始化 资源在对象析构时释放*/
class SqlConnRAII {
public:
    // 传入一个二级的sql指针，一个连接池对象指针
    // 二级指针的作用用于传出数据
    SqlConnRAII(MYSQL** sql, SqlConnPool *connpool) {
        assert(connpool);
        // 获取连接
        *sql = connpool->GetConn();
        // 将这个连接再取指针
        sql_ = *sql;
        connpool_ = connpool;
    }
    
    ~SqlConnRAII() {
        // 用于http连接对象析构时自动释放占用的sql连接，避免占用或者手动释放
        if(sql_) { connpool_->FreeConn(sql_); }
    }
    
private:
    MYSQL *sql_;
    SqlConnPool* connpool_;
};

#endif //SQLCONNRAII_H