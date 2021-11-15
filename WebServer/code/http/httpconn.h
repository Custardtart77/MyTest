#ifndef HTTP_CONN_H
#define HTTP_CONN_H

#include <sys/types.h>
#include <sys/uio.h>     // readv/writev
#include <arpa/inet.h>   // sockaddr_in
#include <stdlib.h>      // atoi()
#include <errno.h>      

#include "../log/log.h"
#include "../pool/sqlconnRAII.h"
#include "../buffer/buffer.h"
#include "httprequest.h"
#include "httpresponse.h"

// Http连接类，其中封装了请求和响应对象
class HttpConn {
public:
    HttpConn();

    ~HttpConn();

    // 连接初始化函数
    void init(int sockFd, const sockaddr_in& addr);

    ssize_t read(int* saveErrno);

    ssize_t write(int* saveErrno);

    void Close();

    int GetFd() const;

    int GetPort() const;

    const char* GetIP() const;
    
    sockaddr_in GetAddr() const;
    
    bool process();

    int ToWriteBytes() { 
        return iov_[0].iov_len + iov_[1].iov_len; 
    }

    bool IsKeepAlive() const {
        return request_.IsKeepAlive();
    }
    // 定义了三个静态成员函数，所有类对象共享
    static bool isET;
    static const char* srcDir;  // 资源的目录
    static std::atomic<int> userCount; // 总共的客户单的连接数
    
private:
    // Socket连接的监听描述符
    int fd_;
    struct  sockaddr_in addr_;

    // 连接是否关闭
    bool isClose_;
    
    int iovCnt_;    // 分散内存的数量
    struct iovec iov_[2];   // 分散内存
    
    // 读写缓冲区也都封装成了一个类，用vector动态数组封装char *, 实现自动增长的缓冲区
    Buffer readBuff_;   // 读(请求)缓冲区，保存请求数据的内容
    Buffer writeBuff_;  // 写(响应)缓冲区，保存响应数据的内容

    // 将请求功能和相应功能都封装成一个类
    // 在构造函数中并没有对着两个类进行初始化
    HttpRequest request_;   // 请求对象
    HttpResponse response_; // 响应对象
};


#endif //HTTP_CONN_H