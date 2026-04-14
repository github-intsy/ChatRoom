#pragma once
#include <functional>
#include <string>
#include <unordered_map>
class Buffer;
class Channel;
class Socket;
class EventLoop;

class Connection
{
private:
    Socket *_sock;
    EventLoop *_loop;
    Channel *_channel;
    Buffer *_readBuffer;
    Buffer *_sendBuffer;
    std::unordered_map<int, const Connection *> *_userConnections;
    std::function<void(Socket *)> _deleteConnectionCallback;
    std::function<std::string(int fd, const std::string &msg,
                              std::unordered_map<int, const Connection *> &userConnection,
                              Connection *currentConnection)>
        _messageCallback;

public:
    Connection();
    Connection(EventLoop *, Socket *, std::unordered_map<int, const Connection *> *userConnection);
    ~Connection();
    void echo(int sockfd);
    void setDeleteConnectionCallback(std::function<void(Socket *)>);
    // 用于调用connection的回调函数，进行业务和连接的分离
    void setMessageCallback(std::function<std::string(int clnt_sock,
                                                      const std::string &clnt_message,
                                                      std::unordered_map<int, const Connection *> &,
                                                      const Connection *currentConnection)>
                                callback);
    void handleRead(int sockfd);
    void handlerWrite();
};