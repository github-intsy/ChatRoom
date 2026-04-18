#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include <mutex>
class Buffer;
class Channel;
class Socket;
class EventLoop;

class Connection
{
private:
    int _userId = -1;
    Socket *_sock;
    EventLoop *_loop;
    Channel *_channel;
    Buffer *_readBuffer;
    Buffer *_sendBuffer;
    std::unordered_map<int, Connection *> *_userConnections;
    std::function<void(Socket *)> _deleteConnectionCallback;
    std::function<std::string(const std::string &msg, Connection *currentConnection,
                              std::unordered_map<int, Connection *> *userConnection)>
        _messageCallback;
    std::mutex _sendMutex;

public:
    Connection();
    Connection(EventLoop *, Socket *, std::unordered_map<int, Connection *> *userConnection);
    ~Connection();
    void echo(int sockfd);
    void setDeleteConnectionCallback(std::function<void(Socket *)>);
    // 用于调用connection的回调函数，进行业务和连接的分离
    void setMessageCallback(std::function<std::string(const std::string &clnt_message,
                                                      Connection *currentConnection, std::unordered_map<int, Connection *> *)>
                                callback);
    void handleRead(int sockfd);
    void handlerWrite();
    void send(const std::string &message);
    void setUserId(int userId);
    int getUserId() const;
};