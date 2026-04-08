#pragma once
#include <map>
class EventLoop;
class Socket;
class Acceptor;
class Connection;

class Server
{
private:
    EventLoop *_loop;                         // 事件循环
    Acceptor *_acceptor;                      // 用于接收tcp连接
    std::map<int, Connection *> _connections; // 所有tcp连接
public:
    Server(EventLoop *);
    ~Server();
    void newConnection(Socket *sock);    // 新建TCP连接
    void deleteConnection(Socket *sock); // 断开TCP连接
};