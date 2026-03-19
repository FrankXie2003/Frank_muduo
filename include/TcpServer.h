#pragma once

#include "nocopyable.h"
#include "EventLoop.h"
#include "Acceptor.h"
#include "InetAddress.h"
#include "EventLoopThreadPool.h"

#include <functional>
#include <string>
#include <memory>

//对外的服务器编程使用的类
class TcpServer : nocopyable
{
public:
    using ThreadInitCallback = std::function<void(EventLoop*)>;
private:
    EventLoop* loop_;//baseLoop用户定义的loop
    const std::string ipPort_;
    const std::string name_;
    std::unique_ptr<Acceptor> accept_;
    std::shared_ptr<EventLoopThreadPool> threadPool_;
};