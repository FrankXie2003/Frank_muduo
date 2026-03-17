#pragma once

#include "nocopyable.h"
#include "Timestamp.h"

#include <functional>
#include <memory>

class EventLoop;
/*
EventLoop，Poller，Channel共同组成Reactor模型中的Demultiplex
Channel理解为通道，封装了sockfd和其感兴趣的event，如EPOLLIN EPOLLOUT
还绑定了poller返回的具体事件
*/
class Channel:nocopyable
{
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Timestamp)>;

    Channel(EventLoop* loop,int fd);
    ~Channel();

    // fd得到Poller通知以后，处理事件的
    void handleEvent(Timestamp receiveTime);

    //设置回调函数对象
    void setReadCallback(ReadEventCallback cb)
    {
        readCallback_ = cb;
    }
        
    void setWriteCallback(EventCallback cb)
    {
        writeCallback_ = cb;
    }
    void setCloseCallback(EventCallback cb)
    {
        closeCallback_ = cb;
    }
    void setErrorCallback(EventCallback cb)
    {
        errorCallback_ = cb;
    }

    //防止当Channel被手动remove掉，Channel还在执行回调操作
    void tie(const std::shared_ptr<void>&);
    int fd() const { return fd_; }
    int events() const { return events_; }
    //poller给channel设置
    void set_revents(int revt) { revents_ = revt; }

    // 设置fd相应的事件状态
    // 将 kReadEvent 标志位添加到 events_ 中
    void enableReading() { events_ |= kReadEvent; update(); }
    void disableReading() { events_ &= ~kReadEvent; update(); }
    void enableWriting() { events_ |= kWriteEvent; update(); }
    void disableWriting() { events_ &= ~kWriteEvent; update(); }
    void disableAll() { events_ = kNoneEvent; }

    //返回fd当前的事件状态
    bool isNoneEvent() const { return events_ == kNoneEvent; }
    bool isWriting() const { return events_ & kWriteEvent; }
    bool isReading() const { return events_ & kReadEvent; }

    int index() { return index_; }
    void set_index(int idx) { index_ = idx; }

    //one loop per thread
    //一个channel肯定属于一个loop，一个loop包含多个channel
    EventLoop* ownerLoop() { return loop_; }
    void remove();

private:

    void update();
    void handleEventWithGuard(Timestamp receiveTime);

    static const int kNoneEvent;
    static const int kReadEvent;
    static const int kWriteEvent;

    EventLoop* loop_;//事件循环
    const int fd_;   //fd,Poller监听的对象  epoll_ctl
    int events_;     //注册fd感兴趣的事件
    int revents_;    //Poller返回的具体发生的时间、
    int index_;

    std::weak_ptr<void> tie_;
    bool tied_;
    
    // 因为Channel通道里面能够获知fd
    //最终发生的具体的事件revents，所以它负责调用具体事件的回调操作
    ReadEventCallback readCallback_;
    EventCallback writeCallback_;
    EventCallback closeCallback_;
    EventCallback errorCallback_;
};