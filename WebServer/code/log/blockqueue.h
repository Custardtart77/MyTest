#ifndef BLOCKQUEUE_H
#define BLOCKQUEUE_H

#include <mutex>
#include <deque>
#include <condition_variable>
#include <sys/time.h>

// 利用类模板定义了一个阻塞队列
// 其实就是对标准的容器库deque双向队列进行封装
template<class T>
class BlockDeque {
public:
    explicit BlockDeque(size_t MaxCapacity = 1000);     // 最大容量

    ~BlockDeque();

    void clear();

    bool empty();

    bool full();

    void Close();

    size_t size();

    size_t capacity();

    T front();

    T back();

    void push_back(const T &item);

    void push_front(const T &item);

    bool pop(T &item);

    bool pop(T &item, int timeout);

    void flush();

private:
    std::deque<T> deq_;

    size_t capacity_;   // 容量

    std::mutex mtx_;    // 互斥锁

    bool isClose_;      // 是否关闭

    std::condition_variable condConsumer_;  // 消费者条件变量，C++内置类型

    std::condition_variable condProducer_;  // 生产者条件变量
};

// 构造函数
template<class T>
BlockDeque<T>::BlockDeque(size_t MaxCapacity) :capacity_(MaxCapacity) {
    assert(MaxCapacity > 0);
    isClose_ = false;
}

template<class T>
BlockDeque<T>::~BlockDeque() {
    Close();
};

template<class T>
void BlockDeque<T>::Close() {
    {   
        std::lock_guard<std::mutex> locker(mtx_);
        deq_.clear();
        isClose_ = true;
    }
    condProducer_.notify_all();
    condConsumer_.notify_all();
};

// 唤醒
template<class T>
void BlockDeque<T>::flush() {
    condConsumer_.notify_one();
};

// 清空队列
template<class T>
void BlockDeque<T>::clear() {
    std::lock_guard<std::mutex> locker(mtx_);
    deq_.clear();
}

// 获取队头
template<class T>
T BlockDeque<T>::front() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.front();
}

// 获取队尾
template<class T>
T BlockDeque<T>::back() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.back();
}

// 获取队列长度
template<class T>
size_t BlockDeque<T>::size() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size();
}

// 获取队列容量
template<class T>
size_t BlockDeque<T>::capacity() {
    std::lock_guard<std::mutex> locker(mtx_);
    return capacity_;
}

// 往前添加，唤醒
template<class T>
void BlockDeque<T>::push_back(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {
        // 容量不足，生产者阻塞等待
        condProducer_.wait(locker);
    }
    deq_.push_back(item);
    condConsumer_.notify_one();
}

// 往后添加，唤醒一个条件变量，跟往前添加一样的思路
template<class T>
void BlockDeque<T>::push_front(const T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.size() >= capacity_) {
        // 容量不足，生产者阻塞等待
        condProducer_.wait(locker);
    }
    deq_.push_front(item);
    condConsumer_.notify_one();
}

template<class T>
bool BlockDeque<T>::empty() {
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.empty();
}

template<class T>
bool BlockDeque<T>::full(){
    std::lock_guard<std::mutex> locker(mtx_);
    return deq_.size() >= capacity_;
}

template<class T>
bool BlockDeque<T>::pop(T &item) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        condConsumer_.wait(locker);
        if(isClose_){
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

template<class T>
bool BlockDeque<T>::pop(T &item, int timeout) {
    std::unique_lock<std::mutex> locker(mtx_);
    while(deq_.empty()){
        if(condConsumer_.wait_for(locker, std::chrono::seconds(timeout)) 
                == std::cv_status::timeout){
            return false;
        }
        if(isClose_){
            return false;
        }
    }
    item = deq_.front();
    deq_.pop_front();
    condProducer_.notify_one();
    return true;
}

#endif // BLOCKQUEUE_H