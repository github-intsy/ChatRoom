#include "Server.h"
#include "Socket.h"
#include "Channel.h"
#include "InetAddress.h"
#include "Acceptor.h"
#include <cstring>
#include <unistd.h>
#include <functional>

#define READ_BUF 1024

// 通过构造函数绑定Channel类和server的newConnection函数
Server::Server(EventLoop *loop)
    : _loop(loop)
    ,_acceptor(nullptr)
{
    _acceptor = new Acceptor(_loop);//创建接收对象
    std::function<void(Socket*)> cb = std::bind(&Server::newConnection, this, std::placeholders::_1);
    _acceptor->setNewConnectionCallback(cb);
}

Server::~Server()
{
    delete _acceptor;
}

// 服务器处理事项,非阻塞io需要不断读取，一次事件读取完毕
void Server::handleReadEvent(int sockfd)
{
    char buf[READ_BUF]; // 定义读取缓冲区
    // 循环读取数据
    while (true)
    {
        bzero(&buf, sizeof(buf)); // 清空缓冲区
        // 从客户端socketfd读取书数据到缓冲区，返回已读取数据大小
        ssize_t read_bytes = read(sockfd, buf, sizeof(buf));
        if (read_bytes > 0)
        {
            printf("message from client fd %d: %s\n", sockfd, buf);
            write(sockfd, buf, sizeof(buf)); // 将获取到的数据写回给客户端
        }
        else if (read_bytes == 0) // read返回0，表示客户端关闭连接，EOF
        {
            printf("client fd %d disconnected\n", sockfd);
            close(sockfd);
            break;
        }
        else if (read_bytes == -1 && errno == EINTR) // 客户端正常中断，继续读取
        {
            printf("continue reading");
            continue;
        }
        // 非阻塞IO，这个条件表示数据全部读取完毕
        else if (read_bytes == -1 && ((errno == EAGAIN) || errno == EWOULDBLOCK))
        {
            printf("finish reading once, errno: %d\n", errno);
            break;
        }
    }
}

//定义客户端连接，通过特定的回调函数进行处理
void Server::newConnection(Socket *serv_sock)
{
    InetAddress *clnt_addr = new InetAddress();
    Socket *clnt_sock = new Socket(serv_sock->accept(clnt_addr));
    printf("new client fd %d! IP: %s Port: %d\n",
           clnt_sock->getfd(), inet_ntoa(clnt_addr->_addr.sin_addr), ntohs(clnt_addr->_addr.sin_port));
    clnt_sock->setnonblocking();
    Channel *clntChannel = new Channel(_loop, clnt_sock->getfd());
    std::function<void()> cb = std::bind(&Server::handleReadEvent, this, clnt_sock->getfd());
    clntChannel->setCallback(cb);
    clntChannel->enableReading();
}
