#pragma once
#include <functional>

class Buffer;
class Channel;
class Socket;
class EventLoop;

class Connection
{
private:
    Socket* _sock;
    EventLoop* _loop;
    Channel* _channel;
    Buffer* _readBuffer;
    std::function<void(Socket*)> _deleteConnectionCallback;
public:

    Connection();
    Connection(EventLoop*, Socket*);
    ~Connection();
    
    void echo(int sockfd);
    void setDeleteConnectionCallback(std::function<void(Socket*)>);
};