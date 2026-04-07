#include "Acceptor.h"
#include "EventLoop.h"
#include "Socket.h"
#include "Channel.h"
#include "InetAddress.h"
void Acceptor::acceptConnection()
{
    //通过Acceptor调用回调函数，将新接受的sock传递给回调函数
    _newConnectionCallback(_sock);
}

Acceptor::Acceptor(EventLoop *loop)
    :_loop(loop)
{
    _sock = new Socket();
    _addr = new InetAddress("0.0.0.0", 8888);
    _sock->bind(_addr); // 绑定ip和端口
    _sock->listen();    // 启动socket监听
    _sock->setnonblocking();

    _acceptChannel = new Channel(_loop, _sock->getfd());
    std::function<void()> cb = std::bind(&Acceptor::acceptConnection, this);
    _acceptChannel->setCallback(cb); // 设置回调函数，指定cb函数调用
    _acceptChannel->enableReading(); // 更新events
}

Acceptor::~Acceptor()
{
    delete _sock;
    delete _addr;
    delete _acceptChannel;
}

void Acceptor::setNewConnectionCallback(std::function<void(Socket *)> cb)
{
    _newConnectionCallback = cb;
}