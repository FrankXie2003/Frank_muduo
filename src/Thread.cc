#include "Thread.h"
#include "CurrentThread.h"

#include <semaphore.h>

std::atomic_int Thread::numCreated_ = 0;

Thread::Thread(ThreadFunc func,const std::string& name)
    :started_(false)
    ,joined_(false)
    ,tid_(0)
    ,func_(std::move(func))
    ,name_(name)
{}

Thread::~Thread()
{
    if(started_ && !joined_)
    {
        thread_->detach();//让它自己跑，结束后自动回收
    }
}

void Thread::start()//一个Thread对象，记录的就是一个新线程的详细信息
{
    started_ = true;
    //引入信号量机制
    //确保新线程完成初始化后，start() 才返回。这样调用方拿到的 tid_ 一定是有效的
    sem_t sem;
    sem_init(&sem,false,0);
    thread_ = std::shared_ptr<std::thread>(new std::thread([&] { 
        //获取线程的tid值
        tid_ = CurrentThread::tid();
        sem_post(&sem);
        //开启一个新线程，专门执行该线程函数
        func_();
     } ));

     sem_wait(&sem);
}

void Thread::join()
{
    joined_ = true;
    thread_->join();
}


void Thread::setDefaultName()
{
    int num = ++numCreated_;
    if(name_.empty())
    {
        char buf[32] = {0};
        snprintf(buf,sizeof(buf),"Thread%d",num);
        name_ = buf;
    }
}