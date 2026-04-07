#pragma once

class EventLoop;
class Socket;
class Acceptor;

class Server
{
private:
    EventLoop* _loop;
    Acceptor* _acceptor;
public:
    Server(EventLoop*);
    ~Server();
    void handleReadEvent(int);
    void newConnection(Socket* serv_sock);
};