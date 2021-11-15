#include "log.h"

using namespace std;

Log::Log() {
    lineCount_ = 0;
    isAsync_ = false;
    writeThread_ = nullptr;
    deque_ = nullptr;
    toDay_ = 0;
    fp_ = nullptr;
}

Log::~Log() {
    if(writeThread_ && writeThread_->joinable()) {
        while(!deque_->empty()) {
            deque_->flush();
        };
        deque_->Close();
        writeThread_->join();
    }
    if(fp_) {
        lock_guard<mutex> locker(mtx_);
        flush();
        fclose(fp_);
    }
}

int Log::GetLevel() {
    lock_guard<mutex> locker(mtx_);
    return level_;
}

void Log::SetLevel(int level) {
    lock_guard<mutex> locker(mtx_);
    level_ = level;
}
// Log::Instance()->init(logLevel, "./log", ".log", logQueSize);
void Log::init(int level = 1, const char* path, const char* suffix,
    int maxQueueSize) {
    isOpen_ = true;
    level_ = level;
    if(maxQueueSize > 0) {
        isAsync_ = true;   // 设定为异步
        if(!deque_) {
            // 如果队列为空，新建一个队列，为string类型
            unique_ptr<BlockDeque<std::string>> newDeque(new BlockDeque<std::string>);
            deque_ = move(newDeque);    // 转化为右值引用，这样才可以将一个对象赋值引用给另一个对象？
            
            std::unique_ptr<std::thread> NewThread(new thread(FlushLogThread));
            writeThread_ = move(NewThread);
        }
    } else {
        isAsync_ = false;   // 设定为同步
    }

    lineCount_ = 0;

    time_t timer = time(nullptr);           // 定义一个时间对象
    struct tm *sysTime = localtime(&timer); // 获取当前时间，返回地址
    struct tm t = *sysTime;                 // 获取时间值
    
    path_ = path;                           // 日志文件路径
    suffix_ = suffix;                       // 日志文件后缀
    char fileName[LOG_NAME_LEN] = {0};      // 文件名
    // 生成文件名
    snprintf(fileName, LOG_NAME_LEN - 1, "%s/%04d_%02d_%02d%s", 
            path_, t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, suffix_);
    toDay_ = t.tm_mday; // 当前日期

    {
        lock_guard<mutex> locker(mtx_);
        buff_.RetrieveAll();      // 清除缓冲区  
        if(fp_) {                 // 关闭旧的日志文件
            flush();                // 刷新
            fclose(fp_);            // 关闭
        }
        // 新建一个日志文件
        fp_ = fopen(fileName, "a");
        if(fp_ == nullptr) {
            mkdir(path_, 0777);     // 若目录不存在，新建一个目录
            fp_ = fopen(fileName, "a");
        } 
        assert(fp_ != nullptr);     // 确保日志创建成功
    }
}

// 往日志文件写日志
void Log::write(int level, const char *format, ...) {
    struct timeval now = {0, 0};
    gettimeofday(&now, nullptr);
    time_t tSec = now.tv_sec;
    struct tm *sysTime = localtime(&tSec);
    struct tm t = *sysTime;
    va_list vaList;

    /* 日志日期 日志行数 */
    if (toDay_ != t.tm_mday || (lineCount_ && (lineCount_  %  MAX_LINES == 0)))
    {
        unique_lock<mutex> locker(mtx_);
        locker.unlock();
        
        char newFile[LOG_NAME_LEN];
        char tail[36] = {0};
        snprintf(tail, 36, "%04d_%02d_%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);

        if (toDay_ != t.tm_mday)
        {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s%s", path_, tail, suffix_);
            toDay_ = t.tm_mday;
            lineCount_ = 0;
        }
        else {
            snprintf(newFile, LOG_NAME_LEN - 72, "%s/%s-%d%s", path_, tail, (lineCount_  / MAX_LINES), suffix_);
        }
        
        locker.lock();
        flush();
        fclose(fp_);
        fp_ = fopen(newFile, "a");
        assert(fp_ != nullptr);
    }

    {
        unique_lock<mutex> locker(mtx_);
        lineCount_++;
        int n = snprintf(buff_.BeginWrite(), 128, "%d-%02d-%02d %02d:%02d:%02d.%06ld ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                    t.tm_hour, t.tm_min, t.tm_sec, now.tv_usec);
                    
        buff_.HasWritten(n);
        AppendLogLevelTitle_(level);

        va_start(vaList, format);
        int m = vsnprintf(buff_.BeginWrite(), buff_.WritableBytes(), format, vaList);
        va_end(vaList);

        buff_.HasWritten(m);
        buff_.Append("\n\0", 2);
        // 写日志
        if(isAsync_ && deque_ && !deque_->full()) {
            deque_->push_back(buff_.RetrieveAllToStr());
        } else {
            fputs(buff_.Peek(), fp_);
        }
        buff_.RetrieveAll();
    }
}

void Log::AppendLogLevelTitle_(int level) {
    switch(level) {
    case 0:
        buff_.Append("[debug]: ", 9);
        break;
    case 1:
        buff_.Append("[info] : ", 9);
        break;
    case 2:
        buff_.Append("[warn] : ", 9);
        break;
    case 3:
        buff_.Append("[error]: ", 9);
        break;
    default:
        buff_.Append("[info] : ", 9);
        break;
    }
}

void Log::flush() {
    if(isAsync_) { 
        deque_->flush(); 
    }
    fflush(fp_);
}

void Log::AsyncWrite_() {
    /* 异步写函数，为一个线程所调用*/
    string str = "";
    while(deque_->pop(str)) {
        // 不停地从阻塞队列中取数据，然后加锁
        lock_guard<mutex> locker(mtx_);
        // 将取到的数据写入日志文件中，fp_为日志文件指针
        fputs(str.c_str(), fp_);
    }
}

Log* Log::Instance() {
    static Log inst;
    return &inst;
}

void Log::FlushLogThread() {
    Log::Instance()->AsyncWrite_();
}