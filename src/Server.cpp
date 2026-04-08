#include "Server.h"
#include "Socket.h"
#include "Acceptor.h"
#include <functional>
#include "Connection.h"
#define READ_BUF 1024

// 通过构造函数绑定Channel类和server的newConnection函数
Server::Server(EventLoop *loop)
    : _loop(loop), _acceptor(nullptr)
{
    _acceptor = new Acceptor(_loop); // 创建接收对象
    std::function<void(Socket *)> cb = std::bind(&Server::newConnection, this, std::placeholders::_1);
    _acceptor->setNewConnectionCallback(cb);
}

Server::~Server()
{
    delete _acceptor;
}

// 创建客户端连接，将客户端连接加入map
void Server::newConnection(Socket *sock)
{
    Connection *conn = new Connection(_loop, sock);
    std::function<void(Socket *)> cb = std::bind(&Server::deleteConnection, this, std::placeholders::_1);
    conn->setDeleteConnectionCallback(cb);
    _connections[sock->getfd()] = conn;
}

//
void Server::deleteConnection(Socket *sock)
{
    Connection *conn = _connections[sock->getfd()];
    _connections.erase(sock->getfd());
    delete conn;
}
