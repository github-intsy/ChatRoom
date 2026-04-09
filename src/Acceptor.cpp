#include "Acceptor.h"
#include "EventLoop.h"
#include "Socket.h"
#include "Channel.h"
#include "InetAddress.h"

Acceptor::Acceptor(EventLoop *loop)
    : _loop(loop), _sock(nullptr), _acceptChannel(nullptr)
{
    _sock = new Socket();
    InetAddress *addr = new InetAddress("0.0.0.0", 8888);
    _sock->bind(addr); // 绑定ip和端口
    _sock->listen();   // 启动socket监听
    // _sock->setnonblocking();

    _acceptChannel = new Channel(_loop, _sock->getfd());
    std::function<void()> cb = std::bind(&Acceptor::acceptConnection, this);
    _acceptChannel->setCallback(cb); // 设置回调函数，指定cb函数调用
    _acceptChannel->enableReading(); // 更新events
    delete addr;
}

Acceptor::~Acceptor()
{
    delete _sock;
    delete _acceptChannel;
}

void Acceptor::acceptConnection()
{
    InetAddress *clnt_addr = new InetAddress();
    Socket *clnt_sock = new Socket(_sock->accept(clnt_addr));
    printf("new client fd %d! IP: %s Port: %d\n",
           clnt_sock->getfd(), inet_ntoa(clnt_addr->_addr.sin_addr), ntohs(clnt_addr->_addr.sin_port));
    clnt_sock->setnonblocking();
    // 通过Acceptor调用回调函数，将新接受的sock传递给回调函数
    _newConnectionCallback(clnt_sock);
    delete clnt_addr;
}

void Acceptor::setNewConnectionCallback(std::function<void(Socket *)> cb)
{
    _newConnectionCallback = cb;
}