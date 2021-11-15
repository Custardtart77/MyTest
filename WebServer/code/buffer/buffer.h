#ifndef BUFFER_H
#define BUFFER_H
#include <cstring>   //perror
#include <iostream>
#include <unistd.h>  // write
#include <sys/uio.h> //readv
#include <vector> //readv
#include <atomic>
#include <assert.h>
class Buffer {
public:
    Buffer(int initBuffSize = 1024);
    ~Buffer() = default;

    size_t WritableBytes() const;       
    size_t ReadableBytes() const ;
    size_t PrependableBytes() const;

    const char* Peek() const;
    void EnsureWriteable(size_t len);
    void HasWritten(size_t len);

    void Retrieve(size_t len);
    void RetrieveUntil(const char* end);

    void RetrieveAll() ;
    std::string RetrieveAllToStr();

    const char* BeginWriteConst() const;
    char* BeginWrite();

    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const Buffer& buff);

    ssize_t ReadFd(int fd, int* Errno); 
    ssize_t WriteFd(int fd, int* Errno);

private:
    char* BeginPtr_();              // 获取内存起始位置
    const char* BeginPtr_() const;  // 获取内存起始位置
    void MakeSpace_(size_t len);    // 创建空间

    // 封装char，不是char * 
    std::vector<char> buffer_;  // 具体装数据的vector
    std::atomic<std::size_t> readPos_;  // 读的位置
    std::atomic<std::size_t> writePos_; // 写的位置
    // 每个 std::atomic 模板的实例化和全特化定义一个原子类型。若一个线程写入原子对象，同时另一线程从它读取，则行为良好定义。
    // std::atomic 既不可复制亦不可移动。
};

#endif //BUFFER_H