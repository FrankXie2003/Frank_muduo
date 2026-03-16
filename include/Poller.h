#pragma once

#include "nocopyable.h"
#include "Timestamp.h"

#include <vector>
#include <unordered_map>

class Channel;
class EventLoop;

//muduo库中多路事件分发器的核心I/O复用模块
class Poller : nocopyable
{
public:
    using ChannelList = std::vector<Channel*>;

    Poller(EventLoop* Loop);
    virtual ~Poller() = default;

    //给所有I/O复用保留统一的接口，派生类必须重写
    virtual Timestamp poll(int timeoutMs,ChannelList* activeChannels) = 0;
    virtual void updateChannel(Channel* channel) = 0;
    virtual void removeChannel(Channel* channel) = 0;

    //判断参数channel是否在当前poller当中
    bool hasChannel(Channel* channel) const;

    //EventLoop可以通过该接口获取默认的I/O复用的具体实现，Poller及其子类继承与多态
    static Poller* newDefaultPoller(EventLoop* loop);
protected:
    //map的key表示sockfd，value表示sockfd所属的channel
    using ChannelMap = std::unordered_map<int,Channel*>;
    ChannelMap channels_;
private:
    EventLoop* ownerLoop_;
    
};