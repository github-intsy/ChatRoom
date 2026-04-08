#pragma once
#include <functional>
class EventLoop;
class Socket;
class Channel;

class Acceptor
{
private:
    EventLoop *_loop;
    Socket *_sock;
    Channel *_acceptChannel;
    std::function<void(Socket *)> _newConnectionCallback;

public:
    Acceptor(EventLoop *_loop);
    ~Acceptor();
    void acceptConnection();
    void setNewConnectionCallback(std::function<void(Socket *)>);
};