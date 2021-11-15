#include "webserver.h"

using namespace std;

WebServer::WebServer(
            int port, int trigMode, int timeoutMS, bool OptLinger,
            int sqlPort, const char* sqlUser, const  char* sqlPwd,
            const char* dbName, int connPoolNum, int threadNum,
            bool openLog, int logLevel, int logQueSize):

            port_(port), openLinger_(OptLinger), timeoutMS_(timeoutMS), isClose_(false),
            // 新建一个定时器对象，一个线程池对象和一颗epoll树，都以new的形式创建，并返回一个指针
            timer_(new HeapTimer()), threadpool_(new ThreadPool(threadNum)), epoller_(new Epoller())
{
    // 以下为构造函数主体部分
    // /home/nowcoder/WebServer-master/
    srcDir_ = getcwd(nullptr, 256); // 获取当前的工作路径
    // 断言函数
    assert(srcDir_);
    // /home/nowcoder/WebServer-master/resources/
    strncat(srcDir_, "/resources/", 16);    // 拼接资源路径

    // 当前所有连接数
    // 初始化连接数和工作
    HttpConn::userCount = 0;
    // 资源文件目录
    HttpConn::srcDir = srcDir_;

    // 初始化数据库连接池
    // Instance()得到一个SqlConnPool一个实例化对象，为静态局部变量
    // 传入参数为主机名，sql端口，sql用户名，sql密码，数据库名，连接池数量（main函数里传入的参数为12）
    SqlConnPool::Instance()->Init("localhost", sqlPort, sqlUser, sqlPwd, dbName, connPoolNum);

    // 初始化事件的模式，一般为边缘触发
    InitEventMode_(trigMode);

    // 初始化网络通信相关的一些内容, isClose为关闭状态
    // 创建监听套接字，可能创建失败
    if(!InitSocket_()) { isClose_ = true;}

    // 引入日志模板功能，先不看
    if(openLog) {
        // 初始化日志信息
        // 日志系统使用的也是单例模式，logLevel = 1， logQueSize = 1024，如果logQueSize为0，则为同步日志系统
        Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
        if(isClose_) { LOG_ERROR("========== Server init error!=========="); }
        else {
            LOG_INFO("========== Server init ==========");
            LOG_INFO("Port:%d, OpenLinger: %s", port_, OptLinger? "true":"false");
            LOG_INFO("Listen Mode: %s, OpenConn Mode: %s",
                            (listenEvent_ & EPOLLET ? "ET": "LT"),
                            (connEvent_ & EPOLLET ? "ET": "LT"));
            LOG_INFO("LogSys level: %d", logLevel);
            LOG_INFO("srcDir: %s", HttpConn::srcDir);
            LOG_INFO("SqlConnPool num: %d, ThreadPool num: %d", connPoolNum, threadNum);
        }
    }
}

// 析构函数，当程序结束时，做一些清除工作
WebServer::~WebServer() {
    close(listenFd_);
    isClose_ = true;
    // 这句啥意思
    free(srcDir_);
    SqlConnPool::Instance()->ClosePool();
}

// 设置监听的文件描述符和通信的文件描述符的模式，这里输入为3，
void WebServer::InitEventMode_(int trigMode) {
    // listenEvent_为监听的文件描述符时间， epoll_event中的epoll时事件
    listenEvent_ = EPOLLRDHUP;  // 代表对端断开连接，不是很理解
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP;  // EPOLLONESHOT 只监听一次事件，当监听完这次事件之后，如果还需要继续监听这个socket的话，需要再次把这个socket加入到EPOLL队列里
    switch (trigMode)
    {
    case 0:
        break;
    case 1:
        connEvent_ |= EPOLLET;
        break;
    case 2:
        listenEvent_ |= EPOLLET;
        break;
    case 3: // 设置监听套接字和连接套接字都是边缘触发事件
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    default:
        listenEvent_ |= EPOLLET;
        connEvent_ |= EPOLLET;
        break;
    }
    // 是否ET模式标志位
    HttpConn::isET = (connEvent_ & EPOLLET);
}

// 启动服务器
void WebServer::Start() {
    int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
    if(!isClose_) { LOG_INFO("========== Server start =========="); }
    // 只要没关闭就一直循环
    while(!isClose_) {

        // 如果设置了超时时间，例如60s,则只要一个连接60秒没有读写操作，则关闭
        if(timeoutMS_ > 0) {
            // 通过定时器GetNextTick(),清除超时的节点，然后获取最先要超时的连接的超时的时间
            timeMS = timer_->GetNextTick();
        }

        // timeMS是最先要超时的连接的超时的时间，传递到epoll_wait()函数中
        // 当timeMS时间内有事件发生，epoll_wait()返回，否则等到了timeMS时间后才返回
        // 这样做的目的是为了让epoll_wait()调用次数变少，提高效率
        int eventCnt = epoller_->Wait(timeMS);

        // 循环处理每一个事件
        for(int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            // 不是直接访问数据，而是通过封装的形式，体现了面向对象编程的特点
            int fd = epoller_->GetEventFd(i);   // 获取事件对应的fd
            uint32_t events = epoller_->GetEvents(i);   // 获取事件的类型

            // 监听的文件描述符有事件，说明有新的连接进来
            if(fd == listenFd_) {
                DealListen_();  // 处理监听的操作，接受客户端连接

            }

            // 错误的一些情况
            else if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                assert(users_.count(fd) > 0);
                CloseConn_(&users_[fd]);    // 关闭连接
            }

            // 有数据到达
            else if(events & EPOLLIN) {
                assert(users_.count(fd) > 0);
                DealRead_(&users_[fd]); // 处理读操作
            }

            // 可以发送数据
            else if(events & EPOLLOUT) {
                assert(users_.count(fd) > 0);
                DealWrite_(&users_[fd]);    // 处理写操作
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

// 发送错误提示信息
void WebServer::SendError_(int fd, const char*info) {
    assert(fd > 0);
    int ret = send(fd, info, strlen(info), 0);
    if(ret < 0) {
        LOG_WARN("send error to client[%d] error!", fd);
    }
    close(fd);
}

// 关闭连接（从epoll中删除，解除响应对象中的内存映射，用户数递减，关闭文件描述符）
void WebServer::CloseConn_(HttpConn* client) {
    assert(client);
    LOG_INFO("Client[%d] quit!", client->GetFd());
    epoller_->DelFd(client->GetFd());
    client->Close();
}

// 添加客户端
void WebServer::AddClient_(int fd, sockaddr_in addr) {
    assert(fd > 0);
    // fd为新的socket文件描述符，用于和客户端通信
    // 每连接一个，就创建一个HttpConn类对象，并进行初始化
    users_[fd].init(fd, addr);
    // 判断是否开启超时
    if(timeoutMS_ > 0) {
        // 添加到定时器对象中，当检测到超时时执行CloseConn_函数进行关闭连接
        timer_->add(fd, timeoutMS_, std::bind(&WebServer::CloseConn_, this, &users_[fd]));
    }
    // 添加到epoll中进行管理
    epoller_->AddFd(fd, EPOLLIN | connEvent_);
    // 设置文件描述符非阻塞
    SetFdNonblock(fd);
    LOG_INFO("Client[%d] in!", users_[fd].GetFd());
}

// 处理监听
void WebServer::DealListen_() {
    struct sockaddr_in addr; // 保存连接的客户端的信息
    socklen_t len = sizeof(addr);
    // 如果监听文件描述符设置的是 ET模式，则需要循环把所有连接处理了
    // 循环把所有连接都处理了
    do {
        // 从已完成连接队列提取新的连接
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        // 已经设置了监听套接字为非阻塞模式了，若提取不到连接了，就会返回
        if(fd <= 0) { return;}

        // 判断是否超过最大连接数
        else if(HttpConn::userCount >= MAX_FD) {
            SendError_(fd, "Server busy!");
            LOG_WARN("Clients is full!");
            return;
        }
        AddClient_(fd, addr);   // 添加客户端
    } while(listenEvent_ & EPOLLET);
}

// 处理读
void WebServer::DealRead_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);   // 延长这个客户端的超时时间
    // 加入到队列中等待线程池中的线程处理（读取数据）
    threadpool_->AddTask(std::bind(&WebServer::OnRead_, this, client));
}

// 处理写
void WebServer::DealWrite_(HttpConn* client) {
    assert(client);
    ExtentTime_(client);// 延长这个客户端的超时时间
    // 加入到队列中等待线程池中的线程处理（写数据）
    threadpool_->AddTask(std::bind(&WebServer::OnWrite_, this, client));
}

// 延长客户端的超时时间
void WebServer::ExtentTime_(HttpConn* client) {
    assert(client);
    if(timeoutMS_ > 0) { timer_->adjust(client->GetFd(), timeoutMS_); }
}

// 这个方法是在子线程中执行的（读取数据）
void WebServer::OnRead_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int readErrno = 0;
    ret = client->read(&readErrno); // 读取客户端的数据，已经将数据放在读缓冲区了
    // EAGAIN提示没有数据可读，请稍后再试
    if(ret <= 0 && readErrno != EAGAIN) {
        CloseConn_(client);
        return;
    }

    // 业务逻辑的处理
    OnProcess(client);
}

// 业务逻辑的处理
void WebServer::OnProcess(HttpConn* client) {
    if(client->process()) {
        // 将文件描述符可写加入监听
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);    
        // 重置事件以确保这个 socket 下一次可读时，其 EPOLLIN 事件能被触发，进而让其他工作线程有机会继续处理这个 socket。
    } else {
        epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLIN);
    }
}

// 写数据
void WebServer::OnWrite_(HttpConn* client) {
    assert(client);
    int ret = -1;
    int writeErrno = 0;
    ret = client->write(&writeErrno);   // 写数据

    // 如果将要写的字节等于0，说明写完了，判断是否要保持连接，保持连接继续去处理
    if(client->ToWriteBytes() == 0) {
        /* 传输完成 */
        if(client->IsKeepAlive()) {
            OnProcess(client);
            return;
        }
    }
    else if(ret < 0) {
        if(writeErrno == EAGAIN) {
            /* 继续传输 */
            epoller_->ModFd(client->GetFd(), connEvent_ | EPOLLOUT);
            return;
        }
    }
    CloseConn_(client);
}

/* Create listenFd */
bool WebServer::InitSocket_() {
    int ret;
    // 定义Socket结构体
    struct sockaddr_in addr;
    // 判断端口是否合法
    if(port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    addr.sin_family = AF_INET;    // AF_INET是大多数用来产生socket的协议，使用TCP或UDP来传输，用IPv4的地址
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 绑定的是通配地址，为什么要转换呢？有4个字节，自然也要转
    // 由于绑定了统配地址，就不需要用inet_pton()将点分十进制串转化为32位网络大端的数据
    addr.sin_port = htons(port_); // 端口号转网络大端数据流

    // 这个优雅开关是干嘛的
    struct linger optLinger = { 0 };

    // 打开优雅关闭
    if(openLinger_) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    // 创建监听套接字，listenFd_已在WebServer的私有成员中定义好
    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    // 判断是否创建成功
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket error!", port_);
        return false;
    }

    // 对监听套接字做一些相关设置，涉及到优雅关闭
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }

    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }
    // 绑定ip和端口到套接字，需要转换为通用套接字结构体类型
    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    // 开始监听，6 排队建立3次握手队列和刚刚建立3次握手队列的链接数和，可以设置多一些，同一时刻可以处理更多请求
    ret = listen(listenFd_, 6);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }

    // 将监听套接字上树
    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    // 设置监听为非阻塞模式
    SetFdNonblock(listenFd_);
    // 打印监听套接字端口号
    LOG_INFO("Server port:%d", port_);
    return true;
}

// 设置文件描述符非阻塞
int WebServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    // int flag = fcntl(fd, F_GETFD, 0);
    // flag = flag  | O_NONBLOCK;
    // // flag  |= O_NONBLOCK;
    // fcntl(fd, F_SETFL, flag);
    // 先获取状态，再或标志位
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}


