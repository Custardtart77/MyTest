#ifndef EPOLLER_H
#define EPOLLER_H

#include <sys/epoll.h> //epoll_ctl()
#include <fcntl.h>  // fcntl()
#include <unistd.h> // close()
#include <assert.h> // close()
#include <vector>
#include <errno.h>

class Epoller {
public:
    // 监听事件数最大为1024
    explicit Epoller(int maxEvent = 1024);

    // 析构函数
    ~Epoller();

    // 添加监听事件
    bool AddFd(int fd, uint32_t events);

    // 修改监听事件
    bool ModFd(int fd, uint32_t events);

    // 删除监听
    bool DelFd(int fd);

    // 超时等待
    int Wait(int timeoutMs = -1);

    // 获取事件
    int GetEventFd(size_t i) const;

    uint32_t GetEvents(size_t i) const;

private:
    int epollFd_;   // epoll_create()创建一个epoll对象，返回值就是epollFd

    std::vector<struct epoll_event> events_;     // 检测到的事件的集合
    /*
    // 描述事件类的结构体
    struct epoll_event {
			__uint32_t events;  // Epoll events
			epoll_data_t data;  // User data variable
		};
		typedef union epoll_data {
			void *ptr;
			int fd;
			uint32_t u32;
			uint64_t u64;
		} epoll_data_t;

    */
};

#endif //EPOLLER_H
