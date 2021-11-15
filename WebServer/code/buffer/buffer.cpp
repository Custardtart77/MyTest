#include "buffer.h"

// 初始化Buffer
Buffer::Buffer(int initBuffSize) : buffer_(initBuffSize), readPos_(0), writePos_(0) {}

// 可以读的数据的大小  写位置 - 读位置，中间的数据就是可以读的大小
size_t Buffer::ReadableBytes() const {  
    return writePos_ - readPos_;
}

// 可以写的数据大小，缓冲区的总大小 - 写位置
size_t Buffer::WritableBytes() const {
    return buffer_.size() - writePos_;
}

// 前面可以用的空间，当前读取到哪个位置，就是前面可以用的空间大小
size_t Buffer::PrependableBytes() const {
    return readPos_;
}

// 读端的指针
const char* Buffer::Peek() const {
    return BeginPtr_() + readPos_;
}

// 读指针向右移动，取回可用的空间
void Buffer::Retrieve(size_t len) {
    assert(len <= ReadableBytes());
    readPos_ += len;
}

//buff.RetrieveUntil(lineEnd + 2);
// 取回至指定位置的可用的空间
void Buffer::RetrieveUntil(const char* end) {
    assert(Peek() <= end );
    Retrieve(end - Peek());
}
// 清零，取回所有的缓冲区
void Buffer::RetrieveAll() {
    // 将buffer_[0]清零，目的何在？
    bzero(&buffer_[0], buffer_.size());
    readPos_ = 0;
    writePos_ = 0;
}
// 将缓冲值一次性读取出来，返回string类型，并清空缓冲区
// 缓冲区大小是固定的
std::string Buffer::RetrieveAllToStr() {
    std::string str(Peek(), ReadableBytes());
    RetrieveAll();
    return str;
}

// 返回写指针
const char* Buffer::BeginWriteConst() const {
    return BeginPtr_() + writePos_;
}

char* Buffer::BeginWrite() {
    return BeginPtr_() + writePos_;
}

// 移动写指针
void Buffer::HasWritten(size_t len) {
    writePos_ += len;
} 

// Append()函数重载

void Buffer::Append(const std::string& str) {
    // str.data()将string类型转化成char *
    Append(str.data(), str.length());
}

void Buffer::Append(const void* data, size_t len) {
    assert(data);
    Append(static_cast<const char*>(data), len);
}

//  Append(buff, len - writable);   buff临时数组，len-writable是临时数组中的数据个数
void Buffer::Append(const char* str, size_t len) {
    assert(str);
    EnsureWriteable(len);
    // 将数据写入缓冲区
    std::copy(str, str + len, BeginWrite());
    HasWritten(len);
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.Peek(), buff.ReadableBytes());
}


// 判断缓冲区剩余空间是否足够，若不足，则扩展空间
void Buffer::EnsureWriteable(size_t len) {
    if(WritableBytes() < len) {
        MakeSpace_(len);
    }
    assert(WritableBytes() >= len);
}

ssize_t Buffer::ReadFd(int fd, int* saveErrno) {
    // 从客户端的连接套接字的fd读取数据
    char buff[65535];   // 临时的数组，保证能够把所有的数据都读出来
    
    struct iovec iov[2];
    /*
    #include <sys/uio.h>
    struct iovec {
        ptr_t iov_base; // Starting address
        size_t iov_len; // Length in bytes 
    };
    */
    // 读取可写的字节数
    const size_t writable = WritableBytes();
    
    /* 分散读， 保证数据全部读完 */
    // 先把数据读入iov[0]， 再把数据读入iov[1]
    iov[0].iov_base = BeginPtr_() + writePos_;
    iov[0].iov_len = writable;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2);
    if(len < 0) {
        // 保存错误码
        *saveErrno = errno;
    }
    else if(static_cast<size_t>(len) <= writable) {
        writePos_ += len;
    }
    else {
        // 将buff的数据复制至缓冲区
        writePos_ = buffer_.size();
        Append(buff, len - writable);
    }
    return len;
}

ssize_t Buffer::WriteFd(int fd, int* saveErrno) {
    // 获取缓冲区可读长度，读取缓冲区，将缓冲区数据写进fd
    size_t readSize = ReadableBytes();
    ssize_t len = write(fd, Peek(), readSize);
    if(len < 0) {
        *saveErrno = errno;
        return len;
    } 
    readPos_ += len;
    return len;
}

char* Buffer::BeginPtr_() {
    // 返回首元素的地址。.begin()返回首元素的迭代器
    return &*buffer_.begin();
}

const char* Buffer::BeginPtr_() const {
    return &*buffer_.begin();
}

// 缓存空间增长
void Buffer::MakeSpace_(size_t len) {
    // 总空间不够，则扩展
    if(WritableBytes() + PrependableBytes() < len) {
        buffer_.resize(writePos_ + len + 1);
    } 
    else {
        // 缓冲区向左移到最开始
        size_t readable = ReadableBytes();
        std::copy(BeginPtr_() + readPos_, BeginPtr_() + writePos_, BeginPtr_());
        readPos_ = 0;
        writePos_ = readPos_ + readable;
        assert(readable == ReadableBytes());
    }
}