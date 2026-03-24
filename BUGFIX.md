# Frank_muduo Bugfix 记录

> C++ 标准从 C++11 升级到 C++17（CMakeLists.txt）

---

## Bug 1 [严重] TcpConnection::send() 跨线程悬空指针

**文件**: `src/TcpConnection.cc:67-85`

**问题**: 跨线程发送时，`std::bind` 绑定了 `buf.c_str()` 这个裸指针。`buf` 是调用方的局部 `string`，`bind` 只拷贝了指针值。当回调在 subLoop 线程异步执行时，原 `string` 可能已析构，导致悬空指针访问。

**修复**: 用 lambda + move capture 拷贝整个 string，保证数据生命周期延续到回调执行时。

```cpp
// Before (BUG):
loop_->runInLoop(std::bind(&TcpConnection::sendInLoop, this, buf.c_str(), buf.size()));

// After (FIX):
std::string msg(buf);
loop_->runInLoop([this, msg = std::move(msg)](){
    sendInLoop(msg.c_str(), msg.size());
});
```

---

## Bug 2 [严重] Timestamp 精度错误 + 线程不安全

**文件**: `src/Timestamp.cc`

**问题**:
1. `time(NULL)` 返回秒级精度，但字段名为 `microSecondsSinceEpoch_`，精度丢失。
2. `localtime()` 不是线程安全的，多线程下共享静态 `struct tm` 会数据竞争。

**修复**:
1. 改用 `gettimeofday()` 获取微秒级时间戳。
2. `localtime()` → `localtime_r()`（线程安全版本）。
3. `toString()` 输出增加微秒字段（`.%06ld`）。

```cpp
// Before:
return Timestamp(time(NULL));                    // 秒，不是微秒
tm *tm_time = localtime(&microSecondsSinceEpoch_); // 线程不安全

// After:
struct timeval tv;
gettimeofday(&tv, nullptr);
return Timestamp(tv.tv_sec * 1000000 + tv.tv_usec); // 真正的微秒

tm tm_time;
localtime_r(&t, &tm_time); // 线程安全
```

---

## Bug 3 [低] Channel.cc include 使用尖括号

**文件**: `src/Channel.cc:2-3`

**问题**: `#include <EventLoop.h>` 和 `#include <Logger.h>` 使用了尖括号（用于系统头文件），项目自身的头文件应该用双引号。虽然 CMake 的 include path 让它能编译过，但语义不正确。

**修复**:
```cpp
// Before:
#include <EventLoop.h>
#include <Logger.h>

// After:
#include "EventLoop.h"
#include "Logger.h"
```

---

## Bug 4 [中] InetAddress::toIpPort() sprintf 缓冲区溢出

**文件**: `src/InetAddress.cc:29`

**问题**: `sprintf(buf+end, ":%u", port)` 没有边界检查，理论上可能越界写入。

**修复**: 改用 `snprintf` 并传入剩余缓冲区大小。

```cpp
// Before:
sprintf(buf+end, ":%u", port);

// After:
snprintf(buf+end, sizeof(buf)-end, ":%u", port);
```

---

## Bug 5 [中] TcpServer::setThreadNum 命名冲突

**文件**: `include/TcpServer.h:37`

**问题**: 两个不同功能的函数都叫 `setThreadNum`，第一个实际上是设置线程初始化回调。C++ 允许重载但语义混乱。

**修复**: 将设置回调的函数重命名为 `setThreadInitCallback`。

```cpp
// Before:
void setThreadNum(const ThreadInitCallback& cb) { threadInitCallback_ = cb; }
void setThreadNum(int numThreads);

// After:
void setThreadInitCallback(const ThreadInitCallback& cb) { threadInitCallback_ = cb; }
void setThreadNum(int numThreads);
```

---

## Bug 6 [中] Acceptor reuseport 参数被忽略

**文件**: `src/Acceptor.cc:28`

**问题**: 构造函数的 `reuseport` 参数没有被使用，无论传入 `true` 还是 `false`，都硬编码为 `setReusePort(true)`。导致 `TcpServer::Option::kNoReusePort` 选项无效。

**修复**:
```cpp
// Before:
acceptSocket_.setReusePort(true);

// After:
acceptSocket_.setReusePort(reuseport);
```

---

## Bug 7 [中] EventLoopThreadPool::start() 使用 VLA

**文件**: `src/EventLoopThreadPool.cc:23`

**问题**: `char buf[name_.size() + 32]` 是 VLA（Variable Length Array），不是标准 C++ 特性，仅作为 GCC 扩展支持，跨编译器时会编译失败。

**修复**: 改用 `std::string`。

```cpp
// Before:
char buf[name_.size() + 32];
snprintf(buf, sizeof(buf), "%s%d", name_.c_str(), i);

// After:
std::string buf = name_ + std::to_string(i);
```

---

## Bug 8 [中] EPollPoller::poll() 过度日志

**文件**: `src/EPollPoller.cc:35`

**问题**: 每次 `epoll_wait` 返回都会 `LOG_INFO` 打印 fd 总数。高并发下每秒可能触发数万次，日志量爆炸，严重影响性能。

**修复**: `LOG_INFO` → `LOG_DEBUG`，仅在开启调试模式时输出。

```cpp
// Before:
LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());

// After:
LOG_DEBUG("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());
```

---

## Bug 9 [低] Thread 类使用 shared_ptr 管理 std::thread

**文件**: `include/Thread.h:32`, `src/Thread.cc:31`

**问题**: `std::shared_ptr<std::thread>` 没有共享所有权的场景，`std::thread` 只被 `Thread` 类独占持有，使用 `shared_ptr` 带来不必要的引用计数开销，且语义不清。

**修复**: 改用 `std::unique_ptr<std::thread>`。

```cpp
// Before:
std::shared_ptr<std::thread> thread_;
thread_ = std::shared_ptr<std::thread>(new std::thread([&]{...}));

// After:
std::unique_ptr<std::thread> thread_;
thread_ = std::make_unique<std::thread>([&]{...});
```

---

## 额外修复: Buffer::readFd() sign-compare 警告

**文件**: `src/Buffer.cc:35`

**问题**: `ssize_t`（有符号）与 `size_t`（无符号）比较，编译器警告 `-Wsign-compare`。此处 `n` 已经确认 `>= 0`，安全转换。

**修复**:
```cpp
// Before:
else if(n <= writable)

// After:
else if(static_cast<size_t>(n) <= writable)
```
