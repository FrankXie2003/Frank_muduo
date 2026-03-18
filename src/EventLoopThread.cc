#include "EventLoopThread.h"
#include "EventLoop.h"

EventLoopThread::EventLoopThread(const ThreadInitCallback& cb,const std::string& name)
                    :loop_(nullptr)
                    ,exiting_(false)
                    ,thread_(std::bind(&EventLoopThread::threadFunc,this),name)
                    ,mutex_()
                    ,cond_()
                    ,callback_(cb)
{}

EventLoopThread::~EventLoopThread()
{
    exiting_ = true;
    if(loop_ != nullptr)
    {
        loop_->quit();
        //确保线程安全退出后，才销毁 EventLoopThread 对象
        thread_.join();
    }
}

EventLoop* EventLoopThread::startLoop()
{
    thread_.start();//启动底层的线程
    EventLoop *loop = nullptr;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        while(loop_ == nullptr)
        {
            cond_.wait(lock);
        }
        loop = loop_;
    }
    return loop;
}

//下面的这个方法，是在单独的新线程里面运行的
void EventLoopThread::threadFunc()
{
    EventLoop loop;//one loop per thread

    if(callback_)
    {
        callback_(&loop);
    }

    {
        std::unique_lock<std::mutex> lock(mutex_);
        loop_ = &loop;
        //notify_one() 保证主线程拿到的 loop_ 一定是有效指针，不是 nullptr
        cond_.notify_one();
    }
    loop.loop();//EventLoop loop => Poller.poll()
    std::unique_lock<std::mutex> lock(mutex_);
    loop_ = nullptr;
}