#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
class ThreadPool {
public:
    /* C++中的explicit关键字只能用于修饰只有一个参数的类构造函数
    它的作用是表明该构造函数是显示的, 而非隐式的, 跟它相对应的另一个关键字是implicit, 意思是隐藏的,类构造函数默认情况下即声明为implicit(隐式). */
    explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {
            assert(threadCount > 0);

            // 创建threadCount个子线程
            // 每个子线程都在循环等待任务
            for(size_t i = 0; i < threadCount; i++) {
                // thread创建线程参数是一个匿名函数
                std::thread([pool = pool_] {

                    // 申请互斥锁
                    // std::unique_lock为锁管理模板类，是对通用mutex的封装
                    std::unique_lock<std::mutex> locker(pool->mtx);
                    while(true) {
                        // 判断是否有任务到来
                        if(!pool->tasks.empty()) {
                            // 从任务队列中取第一个任务，std::move是将左值强制转化为右值引用
                            auto task = std::move(pool->tasks.front());
                            // 移除掉队列中第一个元素
                            pool->tasks.pop();

                            // 取任务的时候是互斥的，只能有一个线程在取任务
                            // 取完任务就解锁
                            locker.unlock();
                            task();

                            // 执行完任务后就加锁
                            locker.lock();
                        } 
                        else if(pool->isClosed) break;

                        // 阻塞等待条件变量，释放互斥锁
                        // 当被唤醒时，即这个函数返回，解除阻塞并重新获取互斥锁
                        else pool->cond.wait(locker);   // 如果队列为空，等待
                    }
                }).detach();// 线程分离
                // 线程分离，线程一旦终止就立刻回收它占用的所有资源，而不保留终止状态。
            }
    }

    ThreadPool() = default;

    ThreadPool(ThreadPool&&) = default;
    
    ~ThreadPool() {
        if(static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;
            }
            // 唤醒条件变量等待的线程
            pool_->cond.notify_all();
        }
    }


    // 函数模板
    template<class F>
    // &&是右值引用，右值引用后续只能拿来放在等号右边，如果放在左边，则会报错
    void AddTask(F&& task) {
        {
            // 采用”资源分配时初始化”(RAII)方法来加锁、解锁，这避免了在临界区中因为抛出异常或return等操作导致没有解锁就退出的问题
            // 在lock_guard对象被析构时，它所管理的mutex对象会自动解锁，不需要程序员手动调用lock和unlock对mutex进行上锁和解锁操作
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(std::forward<F>(task));
        }
        // 多个线程会抢占一个任务，只有一个能抢到
        pool_->cond.notify_one();   // 唤醒一个等待的线程
    }

private:
    // 结构体
    struct Pool {
        // 同一个锁反复使用？
        std::mutex mtx;     // 互斥锁
        std::condition_variable cond;   // 条件变量
        bool isClosed;          // 是否关闭

        // std::function可以取代函数指针的作用，因为它可以延迟函数的执行，特别适合作为回调函数使用。它比普通函数指针更加的灵活和便利
        std::queue<std::function<void()>> tasks;    // 队列（保存的是任务）
    };
    std::shared_ptr<Pool> pool_;  //  池子
};


#endif //THREADPOOL_H