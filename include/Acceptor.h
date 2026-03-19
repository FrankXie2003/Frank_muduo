#pragma once

#include "nocopyable.h"
#include "Socket.h"
#include "Channel.h"

#include <functional>

class EventLoop;
class InetAddress;

class Acceptor : nocopyable
{
public:
    using NewConnectionCallback = std::function<void(int sockfd,const InetAddress&)>;
    Acceptor(EventLoop* loop,const InetAddress& listenAddr,bool reuseport);
    ~Acceptor();

    void setNewConnectionCallback(const NewConnectionCallback& cb)
    {
        newConnectionCallback_ = std::move(cb);
    }

    bool listenning() { return listenning_; }
    void listen();

private:
    void handleRead();

    //Acceptor用的就是用户定义的baseloop(mainloop)
    EventLoop* loop_;
    Socket acceptSocket_;
    Channel acceptChannel_; 
    NewConnectionCallback newConnectionCallback_;
    bool listenning_;

};