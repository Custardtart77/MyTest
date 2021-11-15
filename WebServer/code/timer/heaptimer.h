#ifndef HEAP_TIMER_H
#define HEAP_TIMER_H

#include <queue>
#include <unordered_map>
#include <time.h>
#include <algorithm>
#include <arpa/inet.h> 
#include <functional> 
#include <assert.h> 
#include <chrono>
#include "../log/log.h"

typedef std::function<void()> TimeoutCallBack;
typedef std::chrono::high_resolution_clock Clock;
typedef std::chrono::milliseconds MS;
typedef Clock::time_point TimeStamp;

// 定时器结构体
struct TimerNode {
    int id;
    // 时间戳？
    TimeStamp expires;
    // 定时器到期时的回调函数
    TimeoutCallBack cb;
    // 运算符重载，比较两个定时器
    bool operator<(const TimerNode& t) {
        return expires < t.expires;
    }
};

// 小根堆定时器
class HeapTimer {
public:
    // 构造函数，反转所有节点，为什么这样做？
    HeapTimer() { heap_.reserve(64); }
    // 析构函数，清除所有节点
    ~HeapTimer() { clear(); }
    
    /* 调整指定id的结点，调整为新的时间戳 */
    void adjust(int id, int newExpires);

    // 添加回调时间
    void add(int id, int timeOut, const TimeoutCallBack& cb);
    /* 删除指定id结点，并触发回调函数 */
    void doWork(int id);
    // 清除所有定时器时间，用于析构？
    void clear();
    /* 清除超时结点 */
    void tick();
    // 移除堆顶节点
    void pop();
    // 清除超时节点，获取下一个待超时的节点
    int GetNextTick();

private:
    /* 删除指定位置的结点 */
    void del_(size_t i);
    
    void siftup_(size_t i);  // 向上调整

    bool siftdown_(size_t index, size_t n);
    // 交换两个节点
    void SwapNode_(size_t i, size_t j);

    std::vector<TimerNode> heap_;

    std::unordered_map<int, size_t> ref_;   // key：文件描述符？， value: 位于堆的索引
};

#endif //HEAP_TIMER_H