#pragma once

#include <cstddef>
#include <sys/types.h>
#include <vector>
#include <string>
#include <algorithm>

//网络库底层的缓冲器类型定义
class Buffer
{
public:
    static const std::size_t kCheapPrepend = 8;
    static const std::size_t kInitalSize = 1024;

    explicit Buffer(std::size_t initialSize = kInitalSize)
        : buffer_(kCheapPrepend + kInitalSize)
        , readerIndex_(kCheapPrepend)
        , writerIndex_(kCheapPrepend)
        {}
    
    //可读数据长度
    std::size_t readableBytes() const
    {
        return writerIndex_ - readerIndex_;
    }

    std::size_t writableBytes() const
    {
        return buffer_.size() - writerIndex_;
    }

    std::size_t prependableBytes() const
    {
        return readerIndex_;
    }

    //返回缓冲区中可读数据的起始地址
    const char* peek() const
    {
        return begin() + readerIndex_;
    }

    // onMessage string <- Buffer
    void retrieve(size_t len)
    {
        if(len < readableBytes())
        {
            // 应用只读取了刻度缓冲区数据的一部分，就是len
            // 还剩下readerIndex_ += len -> writerIndex_
            readerIndex_ += len;
        }
        else // len == readableBytes
        {
            retrieveAll();
        }
    }

    void retrieveAll()
    {
        readerIndex_ = writerIndex_ = kCheapPrepend;
    }

    //把onMessage函数上报的Buffer数据，转成string类型的数据返回
    std::string retrieveAllAsString()
    {
        //应用可读取数据的长度
        return retrieveAsString(readableBytes());
    }

    std::string retrieveAsString(std::size_t len)
    {
        std::string result(peek(),len);
        //上面一句把缓冲区中可读的数据已经读取出来
        //这里肯定要对缓冲区进行复位操作
        retrieve(len);
        return result;
    }

    // buffer_.size() - writerIndex_ ? len
    void ensureWritableBytes(size_t len)
    {
        if(writableBytes() < len)
        {
            makeSpace(len);//扩容函数
        }
    }
    
    //把[data, data + len]内存上的数据，添加到writable缓冲区当中
    void append(const char* data, size_t len)
    {
        ensureWritableBytes(len);
        std::copy(data, data + len, beginWrite());
        writerIndex_ += len;
    }

    char* beginWrite()
    {
        return begin() + writerIndex_;
    }

    const char* beginWrite() const
    {
        return begin() + writerIndex_;
    }

    //从fd上读取数据
    ssize_t readFd(int fd, int* saveErrno);
    //通过fd发送数据
    ssize_t writeFd(int fd,int* saveErrno);

private:
    char* begin()
    {
        return &*buffer_.begin();
    }

    const char* begin() const
    {
        return &*buffer_.begin();
    }

    void makeSpace(size_t len)
    {
        if(prependableBytes() + writableBytes() < len + kCheapPrepend)
        {
            buffer_.resize(writerIndex_ + len);
        }
        /*
                          writerIndex_
                  readerInderx_|
                          |    |     
        kCheapPrepend | reader | writer
        kCheapPrepend |       len       |
        */
        else
        {
            size_t readable = readableBytes();
            std::copy(begin() + readerIndex_,
                      begin() + writerIndex_,
                      begin() + kCheapPrepend);
            readerIndex_ = kCheapPrepend;
            writerIndex_ = readerIndex_ + readable;
        }
    }

    std::vector<char> buffer_;
    std::size_t readerIndex_;
    std::size_t writerIndex_;
};