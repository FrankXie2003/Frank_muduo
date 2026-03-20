#include "Buffer.h"

#include <cstddef>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>

/*从fd上读取数据 Poller工作在LT模式
*Buffer缓冲区是有大小的，但是从fd上读数据的时候，却不知道tcp数据最终大小
*/
//用到了readv(writev),分散读，集中写
ssize_t Buffer::readFd(int fd, int* saveErrno)
{
    char extrabuf[65536] = {0};//栈上的内存空间 64K

    struct iovec vec[2];

    //这是Buffer底层缓冲区剩余的可写空间大小
    const size_t writable = writableBytes();
    vec[0].iov_base = begin() + writerIndex_;
    vec[0].iov_len = writable;

    vec[1].iov_base = extrabuf;
    vec[1].iov_len = sizeof(extrabuf);

    const int iovcnt = (writable < sizeof(extrabuf)) ? 2 : 1;
    //返回实际读取字节数
    const ssize_t n = ::readv(fd,vec,iovcnt);
    if(n < 0)
    {
        *saveErrno = errno;
    }
    else if(n <= writable)
    {
        // 数据全部读进了 Buffer 自身
        writerIndex_ += n;
    }
    else
    {
        // Buffer 装不下，溢出部分在 extrabuf 里
        writerIndex_ = buffer_.size();
        // 把 extrabuf 的数据追加进 Buffer
        append(extrabuf, n - writable); 
    }
    return n;
}