#ifndef LOG_H
#define LOG_H

#include <mutex>
#include <string>
#include <thread>
#include <sys/time.h>
#include <string.h>
#include <stdarg.h>           // vastart va_end
#include <assert.h>
#include <sys/stat.h>         //mkdir
#include "blockqueue.h"
#include "../buffer/buffer.h"

class Log {
public:
    void init(int level, const char* path = "./log", 
                const char* suffix =".log",
                int maxQueueCapacity = 1024);

    static Log* Instance();
    static void FlushLogThread();

    void write(int level, const char *format,...);
    void flush();

    int GetLevel();
    void SetLevel(int level);
    bool IsOpen() { return isOpen_; }
    
private:
    Log();
    void AppendLogLevelTitle_(int level);
    virtual ~Log();
    void AsyncWrite_();

private:
    static const int LOG_PATH_LEN = 256;        // 路径长度
    static const int LOG_NAME_LEN = 256;        // 文件名称长度
    static const int MAX_LINES = 50000;         // 最大行数，超过最大行数需要重新新建一个日志文件

    const char* path_;
    const char* suffix_;

    int MAX_LINES_;

    int lineCount_;             // 已经写了多少行了
    int toDay_;                 // 记录当天的日期

    bool isOpen_;
 
    Buffer buff_;               // 同样用到了缓冲区
    int level_;                 // 文件级别
    bool isAsync_;              // 是否异步

    FILE* fp_;                  // 文件指针，用于操作日志文件
    std::unique_ptr<BlockDeque<std::string>> deque_;    // 阻塞队列
    std::unique_ptr<std::thread> writeThread_;          // 写线程
    std::mutex mtx_;                                    // 互斥锁
};

// 宏函数，获取实例，判断是否打开和传入的级别是否足够
// 满足条件，写日志，刷新
#define LOG_BASE(level, format, ...) \
    do {\
        Log* log = Log::Instance();\
        if (log->IsOpen() && log->GetLevel() <= level) {\
            log->write(level, format, ##__VA_ARGS__); \
            log->flush();\
        }\
    } while(0);
// ##__VA_ARGS__用于获取多个参数用的
#define LOG_DEBUG(format, ...) do {LOG_BASE(0, format, ##__VA_ARGS__)} while(0);
#define LOG_INFO(format, ...) do {LOG_BASE(1, format, ##__VA_ARGS__)} while(0);
#define LOG_WARN(format, ...) do {LOG_BASE(2, format, ##__VA_ARGS__)} while(0);
#define LOG_ERROR(format, ...) do {LOG_BASE(3, format, ##__VA_ARGS__)} while(0);

#endif //LOG_H