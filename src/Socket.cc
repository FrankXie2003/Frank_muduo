#include "Socket.h"
#include "Logger.h"
#include "InetAddress.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>

Socket::~Socket()
{
    close(sockfd_);
}

void Socket::bindAddress(const InetAddress& localaddr)
{
    /*
    int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
    sockfd — 要绑定的 socket fd
    addr — 要绑定的本地地址（IP + Port）
    addrlen — addr 结构体的大小
    返回 0 成功，-1 失败
    */
    if(0 != ::bind(sockfd_,(const sockaddr*)localaddr.getSockAddr(),sizeof(sockaddr_in)))
    {
        LOG_FATAL("bind socket:%d fail \n",sockfd_);
    }
}

void Socket::listen()
{
    /*
    int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
    sockfd — 要绑定的 socket fd
    addr — 要绑定的本地地址（IP + Port）
    addrlen — addr 结构体的大小
    返回 0 成功，-1 失败
    */
    if(0 != ::listen(sockfd_,1024))
    {
        LOG_FATAL("listen sockfd:%d fail \n",sockfd_);
    }   
}

int Socket::accept(InetAddress* peeraddr)//要修改所以用指针
{
    sockaddr_in addr;
    socklen_t len;
    bzero(&addr,sizeof(addr));
    /*
    int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
    sockfd — 监听中的 socket fd
    addr — 输出参数，内核填入客户端的地址
    addrlen — 输入输出参数，传入 addr 缓冲区大小，返回实际填入大小
    */
    int connfd = ::accept(sockfd_,(sockaddr*)&addr,&len);
    if(connfd >= 0)
    {
        peeraddr->setSockAddr(addr);
    }
    return connfd;
}

void Socket::shutdownWrite()
{
    if(::shutdown(sockfd_,SHUT_WR) < 0)
    {
        LOG_ERROR("shutdownwrite error");
    }
}

//是否开启某个 socket 选项
/*
setsockopt(sockfd_,  IPPROTO_TCP,  TCP_NODELAY,  &optval,      sizeof(optval))
           ↓         ↓             ↓
           socket fd  协议层        选项名         选项值的指针   选项值的大小

*/
void Socket::setTcpNoDelay(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_,IPPROTO_TCP,TCP_NODELAY,&optval,sizeof(optval));
}

void Socket::setReuseAddr(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_,SOL_SOCKET,SO_REUSEADDR,&optval,sizeof(optval));
}

void Socket::setReusePort(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_,SOL_SOCKET,SO_REUSEPORT,&optval,sizeof(optval));
}

void Socket::setKeepAlive(bool on)
{
    int optval = on ? 1 : 0;
    ::setsockopt(sockfd_,SOL_SOCKET,SO_KEEPALIVE,&optval,sizeof(optval));
}