#include "Connection.h"
#include "Channel.h"
#include "Socket.h"
#include "EventLoop.h"
#include "Buffer.h"
#include <functional>

#define READ_BUF 1024

Connection::Connection(EventLoop *loop, Socket *sock)
    : _loop(loop), _sock(sock), _channel(nullptr), _readBuffer(nullptr)
{
    _channel = new Channel(_loop, _sock->getfd());//获取连接的channel
    _readBuffer = new Buffer();
    std::function<void()> cb = std::bind(&Connection::echo, this, _sock->getfd());
    _channel->setCallback(cb);//绑定回调函数
    _channel->enableReading();//打开读事件监听
}

Connection::~Connection()
{
    delete _channel;
    delete _sock;
    delete _readBuffer;
}

//非阻塞io需要不断读取，一次事件读取完毕
void Connection::echo(int sockfd)
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
            // printf("message from client fd %d: %s\n", sockfd, buf);
            // write(sockfd, buf, sizeof(buf)); // 将获取到的数据写回给客户端
            _readBuffer->append(buf, read_bytes);//获取数据到缓冲区
        }
        else if (read_bytes == 0) // read返回0，表示客户端关闭连接，EOF
        {
            printf("EOF, client fd %d disconnected\n", sockfd);
            // close(sockfd); //关闭socket会自动将文件描述符从epoll树上移除
            _deleteConnectionCallback(_sock);
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
            printf("message from client fd %d: %s\n", sockfd, _readBuffer->c_str());
            errif(write(sockfd, _readBuffer->c_str(), _readBuffer->size()) == -1, "socket write error");
            _readBuffer->clear();
            // printf("finish reading once, errno: %d\n", errno); //11
            break;
        }
    }
}

void Connection::setDeleteConnectionCallback(std::function<void(Socket *)> cb)
{
    _deleteConnectionCallback = cb;
}