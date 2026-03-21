#include "TcpConnection.h"
#include "Callbacks.h"
#include "Logger.h"
#include "Socket.h"
#include "Channel.h"
#include "EventLoop.h"

#include <asm-generic/socket.h>
#include <functional>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include <netinet/tcp.h>

static EventLoop* CheckLoopNotNull(EventLoop* loop)
{   
    if(loop == nullptr)
    {
        LOG_FATAL("%s:%s:%d TcpConnection is null! \n",__FILE__,__FUNCTION__,__LINE__);
    }
    return loop;
}

TcpConnection::TcpConnection(EventLoop* loop,
            const std::string& nameArg,
            int sockfd,
            const InetAddress& localAddr,
            const InetAddress& peerAddr)
    : loop_(CheckLoopNotNull(loop))
    , name_(nameArg)
    , state_(kConnecting)
    , reading_(true)
    , socket_(new Socket(sockfd))
    , channel_(new Channel(loop,sockfd))
    , localAddr_(localAddr)
    , peerAddr_(peerAddr)
    , highWaterMark_(64*1024*1024) // 64M
{
    //下面给channel设置相应的回调函数，poller给channel通知感兴趣的事件发生了，channel会回调相应的操作函数
    channel_->setReadCallback(
        std::bind(&TcpConnection::handleRead,this,std::placeholders::_1)
    );
    channel_->setWriteCallback(
        std::bind(&TcpConnection::handleWrite,this)
    );
    channel_->setCloseCallback(
        std::bind(&TcpConnection::handleClose,this)
    );
    channel_->setErrorCallback(
        std::bind(&TcpConnection::handleError,this)
    );

    LOG_INFO("TcpConnection::ctor[%s] at fd=%d\n",name_.c_str(),sockfd);
    socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
    LOG_INFO("TcpConnection::dtor[%s] at fd=%d state=%d \n",
        name_.c_str(),channel_->fd(),(int)state_);
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
    int savedErrno = 0;
    ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
    if(n > 0)
    {
        //已建立连接的用户，有可读事件发生，调用用户传入的回调操作onMessage
        messageCallback_(shared_from_this(),&inputBuffer_,receiveTime);
    }
    else if(n == 0)
    {
        handleClose();
    }
    else
    {
        errno = savedErrno;
        LOG_ERROR("TcpConnection::handleRead");
        handleError();
    }
}

/*
handleRead 不需要判断 isReading：

当 epoll 返回 EPOLLIN 事件时，说明 fd 上有数据可读，这是一个被动事件——数据来了你就得读，没有什么"我不想读"的场景。只要注册了读事件并触发了，就直接读。
handleWrite 需要判断 isWriting：

写是主动行为。大多数时候 fd 的写缓冲区是空闲的（EPOLLOUT 几乎一直就绪），如果一直监听写事件，epoll 会不停触发，造成 busy loop。
所以 muduo 的做法是：平时不注册 EPOLLOUT，只有当 send() 时发现一次写不完（内核缓冲区满了），才把剩余数据放入 outputBuffer_，同时调用 enableWriting() 注册写事件。
当内核缓冲区有空间了，epoll 触发 EPOLLOUT，进入 handleWrite。这里检查 isWriting() 是一层防御性判断——确认当前确实注册了写事件，防止在 disableWriting() 和 epoll 回调之间出现竞态。
写完后立刻 disableWriting()，避免持续触发。
*/
void TcpConnection::handleWrite()
{
    if(channel_->isWriting())
    {
        int saveErrno = 0;
        ssize_t n =outputBuffer_.writeFd(channel_->fd(),&saveErrno);
        if(n > 0)
        {
            outputBuffer_.retrieve(n);
            if(outputBuffer_.readableBytes() == 0)
            {
                channel_->disableWriting();
                if(writeCompleteCallback_)
                {
                    //唤醒loop_对应的thread线程，执行回调
                    loop_->queueInLoop(
                        std::bind(writeCompleteCallback_,shared_from_this())
                    );
                }
                if(state_ == kDisconnecting)
                {
                    shutdownInLoop();
                }
            }
        }
        else
        {
            LOG_ERROR("TcpConnection::handleWrite");
        }
    }
    else
    {
        LOG_ERROR("TcpConnection fd=%d is down, no more writing \n",channel_->fd());
    }
}

void TcpConnection::handleClose()
{
    LOG_INFO("fd=%d state=%d \n",channel_->fd(),(int)state_);
    setState(kDisconnected);
    channel_->disableAll();

    TcpConnectionPtr connPtr(shared_from_this());
    connectionCallback_(connPtr); //执行连接关闭的回调
    closeCallback_(connPtr); // 关闭连接的回调
}

void TcpConnection::handleError()
{
    int optval;
    socklen_t optlen = sizeof(optval);
    int err = 0;
    if(::getsockopt(channel_->fd(), SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
    {
        err = errno;
    }
    else
    {
        err = optval;
    }
    LOG_ERROR("TcpConnection::handleError name:%s - SO_ERROR:%d \n",name_.c_str(),err);
}