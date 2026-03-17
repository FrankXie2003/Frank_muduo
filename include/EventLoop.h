#pragma once

#include <functional>
#include <vector>
#include <atomic>
#include <memory>
#include <mutex>

#include "nocopyable.h"
#include "Timestamp.h"
#include "CurrentThread.h"

class Channel;
class Poller;
//时间循环类 包含两个大模块 Channel Poller(epoll的抽象)
class EventLoop : nocopyable
{
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    //开启事件循环
    void loop();
    //退出事件循环
    void quit();

    Timestamp pollReturnTime() const { return pollReturnTime_; }

    //在当前loop中执行cb
    void runInLoop(Functor cb);
    //把cb放入队列中，唤醒loop所在的线程执行cb
    void queueInLoop(Functor cb);

    //唤醒loop所在的线程
    void wakeup();

    //EventLoop方法 => Poller方法
    void updateChannel(Channel* channel);
    void removeChannel(Channel* channel);
    bool hasChannel(Channel* channel);

    //判断EventLoop对象是否在自己的线程里面
    bool isInLoopThread() const { return threadId_ == CurrentThread::tid(); }
private:
    void handleRead(); //wake up
    void doPendingFunctors();// 执行回调

    using ChannelList = std::vector<Channel*>;

    std::atomic_bool looping_;//原子操作CAS实现
    std::atomic_bool quit_;//标识退出loop循环
    
    const pid_t threadId_;
    //poller返回发生事件的channel的时间点
    Timestamp pollReturnTime_;
    std::unique_ptr<Poller> poller_;

    //主要作用：当mainloop获取一个新用户的channel，通过轮询算法选择一个subloop
    //通过该成员唤醒subloop处理channel
    //pipe 需要两个 fd（读端+写端），eventfd 只需要一个
    //eventfd 专门为这种"通知"场景设计，更轻量
    int wakeupFd_;
    std::unique_ptr<Channel> wakeupChannel_;

    ChannelList activeChannels_;
    Channel* currentActiveChannel_;

    //标识当前loop是否有需要执行的回调操作
    std::atomic_bool callingPendingFunctors_;
    //存储loop需要执行的所有回调操作
    std::vector<Functor> pendingFunctors_;
    //互斥锁，用来保护上面vector容器的线程安全操作
    std::mutex mutex_;
};