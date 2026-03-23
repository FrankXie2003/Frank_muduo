#include <mymuduo/EventLoop.h>
#include <mymuduo/TcpServer.h>


class EchoServer
{
public:
    EchoServer(EventLoop* loop,
            const InetAddress& addr,
            const std::string& name)
        : server_(loop,addr,name)
        , loop_(loop)
    {
        //注册回调函数

        //设置合适的loop线程数量
    }
private:
    //连接建立或者断开的回调
    void onConnection(const TcpConnectionPtr& conn)
    {
        
    }

    //可读写事件回调
    void onMessage(const TcpConnectionPtr& conn,
                Buffer* buf,
                Timestamp time)
    {}
    EventLoop* loop_;
    TcpServer server_;
};

int main()
{
    return 0;
}