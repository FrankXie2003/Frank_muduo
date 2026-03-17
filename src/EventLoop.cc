#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"

#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
//防止一个线程创建多个EventLoop thread_local
__thread EventLoop* t_loopInThisThread = nullptr;

//定义默认的Poller I/O复用接口的超时时间
const int kPollTimeMs = 10000;

//创建wakeupfd，用来唤醒notify唤醒subReactor处理新来的channel
int createEventfd()
{
    int evtfd = ::eventfd(0,EFD_NONBLOCK | EFD_CLOEXEC);
    if(evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d \n",errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    : looping_(false)
    , quit_(false)
    , callingPendingFunctors_(false)
    , threadId_(CurrentThread::tid())
    , poller_(Poller::newDefaultPoller(this))
    , wakeupFd_(createEventfd())
    , wakeupChannel_(new Channel(this,wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d \n",this,threadId_);
    if(t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop %p exists in this thread %d \n"
                  , t_loopInThisThread,threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }
    //设置wakeupfd的事件类型以及发生
    wakeupChannel_->setReadCallback(std::bind(&EventLoop::handleRead,this));
    //每一个eventloop都将监听wakeupchannel的EPOLLIN读事件了
    wakeupChannel_->enableReading();
}

EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

//开启事件循环
void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping \n",this);

    while(!quit)
    {
        activeChannels_.clear();
        //监听两类fd，一种是client的fd，一种是wakeupfd
        pollReturnTime_ = poller_->poll(kPollTimeMs,&activeChannels_);
        for(Channel* channel : activeChannels_)
        {
            //Poller监听哪些channel发生事件了，然后上报给Eventloop，通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
        //执行当前EventLoop事件循环需要处理的回调操作
        //mainloop事先注册一个回调cb(需要subloop来执行)
        //wakeup subloop后，执行下面的方法
        //执行之前mainloop注册的cb操作
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping \n",this);
    looping_ = false;
}

//退出事件循环 1.loop在自己的线程中调用quit
//2.在非loop的线程中，调用loop的quit
void EventLoop::quit()
{
    quit_ = true;
    //如果是在其他线程中调用的quit
    //在一个subloop(worker)中，调用了mainloop(I/O)的quit
    if(!isInLoopThread)
    {
        wakeup();
    }
}

void EventLoop::handleRead()
{
    uint64_t one = 1;
    ssize_t n = read(wakeupFd_,&one,sizeof(one));
    if( n != sizeof(one) )
    {
        LOG_ERROR(" EventLoop::handleRead() reads %d bytes instead of 8",n);
    }
}