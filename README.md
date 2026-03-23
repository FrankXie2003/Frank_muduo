# Frank_muduo - 基于 Reactor 模式的高性能网络库

## 项目简介

Frank_muduo 是一个基于 C++11 实现的高性能网络库，采用 Reactor 模式和多线程模型，支持高并发 TCP 连接。本项目参考了陈硕的 muduo 网络库设计思想，使用 epoll 作为 I/O 多路复用机制，实现了 one loop per thread 的线程模型。

### 核心特性

- **Reactor 模式**：基于事件驱动的网络编程模型
- **多线程模型**：主从 Reactor 模式，one loop per thread
- **非阻塞 I/O**：使用 epoll ET 模式
- **智能指针管理**：使用 shared_ptr/weak_ptr 管理对象生命周期
- **高性能缓冲区**：自动扩容的应用层缓冲区
- **线程安全**：使用 mutex 和 atomic 保证线程安全

### 技术栈

- C++11
- Linux epoll
- POSIX 线程库
- CMake 构建系统

---

## 目录

1. [整体架构](#整体架构)
2. [核心类详解](#核心类详解)
3. [关键流程](#关键流程)
4. [编译与使用](#编译与使用)
5. [示例程序](#示例程序)

---

## 整体架构

### 架构图

┌─────────────────────────────────────────────────────────────────┐
│                          用户代码层                              │
│                      (EchoServer, etc.)                         │
└────────────────────────┬────────────────────────────────────────┘
│
↓
┌─────────────────────────────────────────────────────────────────┐
│                        TcpServer                                │
│  ┌──────────────┐  ┌──────────────┐  ┌────────────────────┐   │
│  │  Acceptor    │  │EventLoopThread│  │ TcpConnection      │   │
│  │  (mainLoop)  │  │     Pool      │  │ (subLoop管理)      │   │
│  └──────────────┘  └──────────────┘  └────────────────────┘   │
└────────────────────────┬────────────────────────────────────────┘
│
↓
┌─────────────────────────────────────────────────────────────────┐
│                      EventLoop 层                               │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  mainLoop (主线程)                                        │  │
│  │  - 负责监听新连接 (Acceptor)                              │  │
│  │  - 分发连接到 subLoop                                     │  │
│  └──────────────────────────────────────────────────────────┘  │
│  ┌──────────────────────────────────────────────────────────┐  │
│  │  subLoop (工作线程池)                                     │  │
│  │  - 负责已建立连接的 I/O 事件处理                          │  │
│  │  - 每个线程一个 EventLoop                                 │  │
│  └──────────────────────────────────────────────────────────┘  │
└────────────────────────┬────────────────────────────────────────┘
│
↓
┌─────────────────────────────────────────────────────────────────┐
│                    Channel & Poller 层                          │
│  ┌──────────────┐         ┌──────────────┐                     │
│  │   Channel    │ ◄─────► │   Poller     │                     │
│  │  (fd封装)    │         │  (epoll封装)  │                     │
│  └──────────────┘         └──────────────┘                     │
└────────────────────────┬────────────────────────────────────────┘
│
↓
┌─────────────────────────────────────────────────────────────────┐
│                      操作系统层                                  │
│                   (epoll, socket, etc.)                         │
└─────────────────────────────────────────────────────────────────┘



### 线程模型

主线程 (mainLoop)                工作线程池 (subLoop)
│                                  │
│  监听 listenfd                   │
│  ↓                               │
│  accept() 新连接                 │
│  ↓                               │
│  轮询选择一个 subLoop            │
│  ↓                               │
├──────────────────────────────────┤
│                                  │
│                            ┌─────┴─────┐
│                            │           │
│                         subLoop1   subLoop2 ... subLoopN
│                            │           │
│                         处理连接1   处理连接2
│                         的I/O事件   的I/O事件



### EventLoop 与 Channel、Poller 的关系

┌─────────────────────────────────────────┐
│           Thread (线程)                  │
│  ┌───────────────────────────────────┐  │
│  │      EventLoop (事件循环)         │  │
│  │  ┌─────────────────────────────┐  │  │
│  │  │   Poller (封装 epoll)       │  │  │
│  │  │   - epollfd_ (一个！)       │  │  │
│  │  │   - channels_ (map)         │  │  │
│  │  └─────────────────────────────┘  │  │
│  │                                    │  │
│  │  ┌─────────────────────────────┐  │  │
│  │  │   ChannelList (活跃的)      │  │  │
│  │  │   - Channel1 (fd1)          │  │  │
│  │  │   - Channel2 (fd2)          │  │  │
│  │  │   - Channel3 (fd3)          │  │  │
│  │  │   - ...                     │  │  │
│  │  └─────────────────────────────┘  │  │
│  └───────────────────────────────────┘  │
└─────────────────────────────────────────┘



---

## 核心类详解

### 1. TcpServer - 服务器类

**作用**：对外的服务器编程接口，管理连接的建立和分发。

**核心成员**：
- `Acceptor* acceptor_`：监听新连接
- `EventLoopThreadPool* threadPool_`：工作线程池
- `ConnectionMap connections_`：管理所有已建立的连接

**关键方法**：
- `start()`：启动服务器，开始监听
- `newConnection()`：新连接到来时的回调
- `removeConnection()`：移除连接

**调用链**：
用户调用 server.start()
→ Acceptor::listen()
→ EventLoop::loop()
→ 有新连接时 Acceptor::handleRead()
→ TcpServer::newConnection()
→ 创建 TcpConnection
→ 选择一个 subLoop
→ TcpConnection::connectEstablished()



### 2. EventLoop - 事件循环类

**作用**：事件循环的核心，one loop per thread，每个线程一个 EventLoop。

**核心成员**：
- `Poller* poller_`：I/O 多路复用器（epoll 封装）
- `ChannelList activeChannels_`：活跃的 Channel 列表
- `int wakeupFd_`：用于唤醒 EventLoop 的 eventfd
- `std::vector<Functor> pendingFunctors_`：待执行的回调队列

**关键方法**：
- `loop()`：开启事件循环
- `runInLoop()`：在当前 loop 中执行回调
- `queueInLoop()`：把回调放入队列，唤醒 loop 执行
- `wakeup()`：唤醒 loop 所在的线程

**设计要点**：
- 使用 `threadId_` 判断是否在自己的线程中
- 使用 `wakeupFd_` 和 `eventfd` 实现线程间唤醒
- 使用 `pendingFunctors_` 队列实现跨线程调用

### 3. Channel - 通道类

**作用**：封装 fd 和其感兴趣的事件，以及事件发生时的回调函数。

**核心成员**：
- `int fd_`：文件描述符
- `int events_`：关注的事件（EPOLLIN、EPOLLOUT 等）
- `int revents_`：实际发生的事件
- `ReadEventCallback readCallback_`：读事件回调
- `EventCallback writeCallback_`：写事件回调
- `std::weak_ptr<void> tie_`：用于延长对象生命周期

**关键方法**：
- `handleEvent()`：根据 revents_ 调用相应的回调
- `tie()`：绑定 TcpConnection，防止在回调中被销毁
- `enableReading()`/`enableWriting()`：注册读/写事件

**tie 机制**：
```cpp
channel_->tie(shared_from_this());  // TcpConnection 构造时

// 在 handleEvent 中
std::shared_ptr<void> guard = tie_.lock();  // 提升为 shared_ptr
if(guard) {
    handleEventWithGuard();  // 保证对象在回调期间不被销毁
}
4. Poller - I/O 多路复用抽象类
作用：封装 epoll，提供统一的 I/O 多路复用接口。

核心成员：

int epollfd_：epoll 文件描述符
std::map<int, Channel*> channels_：fd 到 Channel 的映射
关键方法：

poll()：调用 epoll_wait，返回活跃的 Channel
updateChannel()：更新 Channel 的事件（epoll_ctl）
removeChannel()：移除 Channel
EPollPoller 实现：


Timestamp EPollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
    int numEvents = ::epoll_wait(epollfd_, &*events_.begin(),
                                  static_cast<int>(events_.size()), timeoutMs);
    // 填充 activeChannels
    fillActiveChannels(numEvents, activeChannels);
    return Timestamp::now();
}
5. TcpConnection - TCP 连接类
作用：管理一个 TCP 连接，封装 socket fd 和读写缓冲区。

核心成员：

Socket* socket_：封装的 socket
Channel* channel_：对应的 Channel
Buffer inputBuffer_：接收缓冲区
Buffer outputBuffer_：发送缓冲区
ConnectionCallback connectionCallback_：连接建立/断开回调
MessageCallback messageCallback_：消息到达回调
关键方法：

connectEstablished()：连接建立时调用
connectDestroyed()：连接销毁时调用
send()：发送数据
shutdown()：关闭写端
handleRead()：处理读事件
handleWrite()：处理写事件
生命周期管理：

使用 shared_ptr<TcpConnection> 管理
使用 enable_shared_from_this 获取自身的 shared_ptr
Channel 使用 tie() 机制防止在回调中被销毁
6. Acceptor - 接受器类
作用：监听新连接，accept 后回调 TcpServer。

核心成员：

Socket acceptSocket_：监听 socket
Channel acceptChannel_：监听 Channel
NewConnectionCallback newConnectionCallback_：新连接回调
关键方法：

listen()：开始监听
handleRead()：有新连接时调用 accept
7. EventLoopThreadPool - 事件循环线程池
作用：管理工作线程池，实现 one loop per thread。

核心成员：

std::vector<EventLoopThread*> threads_：线程列表
std::vector<EventLoop*> loops_：EventLoop 列表
int next_：轮询索引
关键方法：

start()：启动线程池
getNextLoop()：轮询获取下一个 EventLoop
8. Buffer - 缓冲区类
作用：应用层缓冲区，自动扩容。

核心成员：

std::vector<char> buffer_：缓冲区
size_t readerIndex_：读指针
size_t writerIndex_：写指针
关键方法：

readableBytes()：可读字节数
writableBytes()：可写字节数
retrieve()：取出数据
append()：追加数据
readFd()：从 fd 读取数据到缓冲区
设计要点：

使用 readv + 栈上临时缓冲区实现高效读取
自动扩容，避免频繁分配
关键流程
1. 服务器启动流程

用户代码：
  EventLoop loop;
  TcpServer server(&loop, addr, "EchoServer");
  server.start();
  loop.loop();

内部流程：
  1. TcpServer 构造
     → 创建 Acceptor
     → 创建 EventLoopThreadPool
     → 设置 newConnectionCallback

  2. server.start()
     → threadPool_->start()  // 启动工作线程
     → acceptor_->listen()   // 开始监听

  3. loop.loop()
     → while(!quit_) {
         poller_->poll()      // epoll_wait
         处理活跃的 Channel
         执行 pendingFunctors
       }
2. 新连接建立流程

1. 客户端连接到来
   → listenfd 可读
   → Poller 返回 acceptChannel

2. acceptChannel->handleEvent()
   → Acceptor::handleRead()
   → accept() 获取 connfd

3. Acceptor::newConnectionCallback_()
   → TcpServer::newConnection()
   → 创建 TcpConnection 对象
   → 选择一个 subLoop（轮询）
   → 设置回调函数

4. 在 subLoop 中执行
   → TcpConnection::connectEstablished()
   → channel_->tie(shared_from_this())
   → channel_->enableReading()
   → connectionCallback_()  // 用户的连接建立回调
3. 消息读取流程

1. connfd 可读
   → Poller 返回对应的 Channel

2. channel->handleEvent()
   → TcpConnection::handleRead()
   → inputBuffer_.readFd()  // 读取数据到缓冲区

3. messageCallback_()
   → 用户的消息回调
   → 处理 inputBuffer_ 中的数据
4. 消息发送流程

1. 用户调用 conn->send(data)

2. TcpConnection::send()
   → 判断是否在 loop 线程
   → 如果在，直接调用 sendInLoop()
   → 如果不在，queueInLoop(sendInLoop)

3. TcpConnection::sendInLoop()
   → 尝试直接 write()
   → 如果没写完，放入 outputBuffer_
   → channel_->enableWriting()  // 注册写事件

4. connfd 可写
   → TcpConnection::handleWrite()
   → 从 outputBuffer_ 读取数据
   → write() 发送
   → 如果全部发送完，channel_->disableWriting()
5. 连接关闭流程

1. 用户调用 conn->shutdown()
   → TcpConnection::shutdown()
   → shutdownInLoop()
   → socket_->shutdownWrite()  // 关闭写端

2. 对端关闭连接
   → connfd 可读，但 read() 返回 0
   → TcpConnection::handleRead()
   → handleClose()

3. TcpConnection::handleClose()
   → channel_->disableAll()
   → closeCallback_()
   → TcpServer::removeConnection()

4. TcpServer::removeConnection()
   → connections_.erase(conn)
   → queueInLoop(connectDestroyed)

5. TcpConnection::connectDestroyed()
   → channel_->remove()
   → connectionCallback_()  // 用户的连接断开回调
6. 跨线程调用流程

场景：在线程 A 中调用线程 B 的 EventLoop 执行任务

1. 线程 A 调用
   → loopB->runInLoop(functor)

2. EventLoop::runInLoop()
   → 判断 isInLoopThread()
   → 如果不在，调用 queueInLoop()

3. EventLoop::queueInLoop()
   → pendingFunctors_.push_back(functor)
   → wakeup()  // 唤醒线程 B

4. EventLoop::wakeup()
   → write(wakeupFd_, &one, sizeof(one))
   → 触发 wakeupChannel_ 的读事件

5. 线程 B 的 EventLoop::loop()
   → poller_->poll() 返回（wakeupFd_ 可读）
   → handleRead()  // 读取 wakeupFd_
   → doPendingFunctors()  // 执行队列中的任务
编译与使用
编译库

# 1. 克隆项目
git clone <your-repo-url>
cd Frank_muduo

# 2. 使用自动编译脚本
./autobuild.sh

# 或者手动编译
mkdir build && cd build
cmake ..
make

# 3. 安装到系统目录（需要 sudo）
sudo cp lib/libFrank_muduo.so /usr/lib/
sudo cp -r include/* /usr/include/mymuduo/
sudo ldconfig
使用库
1. 包含头文件


#include <mymuduo/TcpServer.h>
#include <mymuduo/EventLoop.h>
#include <mymuduo/InetAddress.h>
2. 编译链接


g++ -o myserver myserver.cpp -lFrank_muduo -lpthread
示例程序
Echo 服务器（长连接）

#include <mymuduo/TcpServer.h>
#include <mymuduo/EventLoop.h>
#include <mymuduo/InetAddress.h>
#include <mymuduo/Logger.h>

class EchoServer
{
public:
    EchoServer(EventLoop* loop, const InetAddress& addr, const std::string& name)
        : server_(loop, addr, name)
        , loop_(loop)
    {
        // 注册回调函数
        server_.setConnectionCallback(
            std::bind(&EchoServer::onConnection, this, std::placeholders::_1));
        
        server_.setMessageCallback(
            std::bind(&EchoServer::onMessage, this,
                     std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
        
        // 设置线程数量
        server_.setThreadNum(3);
    }
    
    void start()
    {
        server_.start();
    }

private:
    // 连接建立或断开的回调
    void onConnection(const TcpConnectionPtr& conn)
    {
        if(conn->connected())
        {
            LOG_INFO("Connection UP : %s", conn->peerAddress().toIpPort().c_str());
        }
        else
        {
            LOG_INFO("Connection DOWN : %s", conn->peerAddress().toIpPort().c_str());
        }
    }
    
    // 消息到达的回调
    void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
    {
        std::string msg = buf->retrieveAllAsString();
        LOG_INFO("Received %lu bytes from %s", 
                 msg.size(), conn->peerAddress().toIpPort().c_str());
        
        // 回显消息
        conn->send(msg);
    }
    
    EventLoop* loop_;
    TcpServer server_;
};

int main()
{
    EventLoop loop;
    InetAddress addr(8000);
    
    EchoServer server(&loop, addr, "EchoServer");
    server.start();
    
    LOG_INFO("EchoServer started on port 8000");
    loop.loop();  // 启动事件循环
    
    return 0;
}
测试

# 编译
cd example
make

# 运行服务器
./testserver

# 在另一个终端测试
telnet localhost 8000
# 或
nc localhost 8000
项目结构

Frank_muduo/
├── include/              # 头文件
│   ├── TcpServer.h
│   ├── EventLoop.h
│   ├── Channel.h
│   ├── Poller.h
│   ├── EPollPoller.h
│   ├── TcpConnection.h
│   ├── Acceptor.h
│   ├── Socket.h
│   ├── InetAddress.h
│   ├── Buffer.h
│   ├── EventLoopThread.h
│   ├── EventLoopThreadPool.h
│   ├── Thread.h
│   ├── CurrentThread.h
│   ├── Timestamp.h
│   ├── Logger.h
│   ├── Callbacks.h
│   └── nocopyable.h
├── src/                  # 源文件
│   ├── TcpServer.cc
│   ├── EventLoop.cc
│   ├── Channel.cc
│   ├── Poller.cc
│   ├── EPollPoller.cc
│   ├── DefaultPoller.cc
│   ├── TcpConnection.cc
│   ├── Acceptor.cc
│   ├── Socket.cc
│   ├── InetAddress.cc
│   ├── Buffer.cc
│   ├── EventLoopThread.cc
│   ├── EventLoopThreadPool.cc
│   ├── Thread.cc
│   ├── CurrentThread.cc
│   ├── Timestamp.cc
│   └── Logger.cc
├── example/              # 示例程序
│   ├── testserver.cc
│   └── Makefile
├── lib/                  # 编译生成的库文件
│   └── libFrank_muduo.so
├── build/                # CMake 构建目录
├── CMakeLists.txt        # CMake 配置文件
├── autobuild.sh          # 自动编译脚本
└── README.md             # 本文档
设计亮点
1. Reactor 模式
采用主从 Reactor 模式：

主 Reactor：mainLoop，负责监听新连接
从 Reactor：subLoop 线程池，负责已建立连接的 I/O 事件
2. One Loop Per Thread
每个线程一个 EventLoop，避免锁竞争：

每个 EventLoop 独立运行在一个线程中
使用 threadId_ 判断是否在自己的线程中
跨线程调用使用 queueInLoop() + wakeup()
3. 智能指针管理生命周期
使用 shared_ptr 和 weak_ptr 管理对象生命周期：

TcpConnection 使用 shared_ptr 管理
Channel 使用 weak_ptr + tie() 机制防止在回调中被销毁
4. 高效的缓冲区设计
Buffer 类使用 readv + 栈上临时缓冲区：

减少系统调用次数
自动扩容，避免频繁分配
使用 readerIndex_ 和 writerIndex_ 管理读写位置
5. 线程间通信
使用 eventfd 实现线程间唤醒：

每个 EventLoop 有一个 wakeupFd_
跨线程调用时，write wakeupFd_ 唤醒目标线程
目标线程的 epoll_wait 返回，执行 pendingFunctors_
性能优化
ET 模式：使用 epoll ET 模式，减少事件触发次数
非阻塞 I/O：所有 socket 都设置为非阻塞
应用层缓冲区：减少系统调用，提高吞吐量
线程池：避免频繁创建销毁线程
对象池：复用 TcpConnection 对象（可扩展）
注意事项
线程安全：

EventLoop 的方法只能在自己的线程中调用
跨线程调用必须使用 runInLoop() 或 queueInLoop()
对象生命周期：

TcpConnection 使用 shared_ptr 管理
不要在回调中直接删除对象
回调函数：

回调函数不要阻塞，否则会影响整个 EventLoop
耗时操作应该放到线程池中执行
资源限制：

注意文件描述符限制（ulimit -n）
注意线程数量，避免过多线程导致上下文切换开销
许可证
MIT License

参考资料
陈硕《Linux 多线程服务端编程：使用 muduo C++ 网络库》
muduo 网络库：https://github.com/chenshuo/muduo
Linux epoll 文档：man 7 epoll
作者
Frank

致谢
感谢陈硕老师的 muduo 网络库，本项目参考了其设计思想和实现细节。



---