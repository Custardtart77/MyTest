#ifndef WEBSERVER_H
#define WEBSERVER_H

// 系统头文件
#include <unordered_map> // C++风格的头文件
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>       // 包含定义的表示不同错误码的宏
#include <sys/socket.h>  // Linux的Socket编程
#include <netinet/in.h>  // 包含Ipv4的结构体
#include <arpa/inet.h>   // 网络字节序转换 

// 自定义头文件
#include "epoller.h"
#include "../log/log.h"
#include "../timer/heaptimer.h"
#include "../pool/sqlconnpool.h"
#include "../pool/threadpool.h"
#include "../pool/sqlconnRAII.h"
#include "../http/httpconn.h"

class WebServer {
public:
    // 构造函数，传入参数为：端口号，epoll触发方式，超时时间，
    WebServer(
        int port, int trigMode, int timeoutMS, bool OptLinger,
        int sqlPort, const char* sqlUser, const  char* sqlPwd,
        const char* dbName, int connPoolNum, int threadNum,
        bool openLog, int logLevel, int logQueSize);
        /* 连接池数量 线程池数量 日志开关 日志等级 日志异步队列容量 */
    ~WebServer();
    // 启动函数
    void Start();

private:
    bool InitSocket_();  // 初始化Socket
    void InitEventMode_(int trigMode);  // 设置时间触发方式，有边缘触发和水平触发
    void AddClient_(int fd, sockaddr_in addr);  // 添加客户端

    void DealListen_();                     // 处理监听
    void DealWrite_(HttpConn* client);      // 处理写事件
    void DealRead_(HttpConn* client);       // 处理读时间

    void SendError_(int fd, const char*info);   // 发送错误信息
    void ExtentTime_(HttpConn* client);         // 延时函数？
    void CloseConn_(HttpConn* client);          // 关闭连接

    void OnRead_(HttpConn* client);
    void OnWrite_(HttpConn* client);
    void OnProcess(HttpConn* client);

    static const int MAX_FD = 65536;    // 最大的文件描述符的个数

    static int SetFdNonblock(int fd);   // 设置文件描述符非阻塞

    int port_;          // 端口
    bool openLinger_;   // 是否打开优雅关闭
    int timeoutMS_;  /* 毫秒MS */
    bool isClose_;  // 是否关闭
    int listenFd_;  // 监听的文件描述符
    char* srcDir_;  // 资源的目录

    uint32_t listenEvent_;  // 监听的文件描述符的事件
    uint32_t connEvent_;    // 连接的文件描述符的事件

    // 这三个 都是以指针的形式存在
    std::unique_ptr<HeapTimer> timer_;  // 定时器
    std::unique_ptr<ThreadPool> threadpool_;    // 线程池
    std::unique_ptr<Epoller> epoller_;      // epoll对象
    std::unordered_map<int, HttpConn> users_;   // 哈希表保存的是客户端连接的信息
};


#endif //WEBSERVER_H
