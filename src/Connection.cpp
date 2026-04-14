#include "Connection.h"
#include "Buffer.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Socket.h"
#include <cstring>
#include <functional>
#define READ_BUF 1024

Connection::Connection(EventLoop *loop, Socket *sock, std::unordered_map<int, const Connection *> *userConnections)
    : _sock(sock), _loop(loop), _channel(nullptr), _sendBuffer(new Buffer()),
      _readBuffer(new Buffer()), _userConnections(userConnections)
{
    _channel = new Channel(_loop, _sock->getfd()); // 获取连接的channel
    std::function<void()> rcb = std::bind(&Connection::handleRead, this, _sock->getfd());
    std::function<void()> wcb = std::bind(&Connection::handlerWrite, this);
    _channel->setReadCallback(rcb);  // 绑定回调函数
    _channel->setWriteCallback(wcb); // 绑定写回调函数
    _channel->enableReading();       // 打开读事件监听
    _channel->useET();
}

Connection::~Connection()
{
    delete _channel;
    delete _sock;
    delete _readBuffer;
    delete _sendBuffer;
}

// 非阻塞io需要不断读取，一次事件读取完毕
// 处理读事件
void Connection::handleRead(int sockfd)
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
            _readBuffer->append(buf, read_bytes); // 获取数据到缓冲区
            printf("数据内容是%s, 大小是%d\n", _readBuffer->c_str(), read_bytes);
        }
        else if (read_bytes == 0) // read返回0，表示客户端关闭连接，EOF
        {
            printf("EOF, client fd %d disconnected\n", sockfd);
            // close(sockfd); //关闭socket会自动将文件描述符从epoll树上移除
            _loop->deleteChannel(_channel); // 先将channel从eoll上面移除
            _loop->queueInloop([this]()
                               { _deleteConnectionCallback(_sock); });
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
            // errif(write(sockfd, _readBuffer->c_str(), _readBuffer->size()) == -1,
            // "socket write error");
            if (_messageCallback)
            { // 业务处理回调
                printf("启动事务处理\n");
                // 将事务处理完的结果返回给发送缓冲区，触发EPOLLOUT事件
                printf("接受的内容是%s\n", _readBuffer->c_str());
                std::string full_message = _readBuffer->getBuffer();
                // std::string response = _readBuffer->getBuffer();
                std::string response = _messageCallback(_sock->getfd(), full_message, *_userConnections, this);
                printf("response is: %s, size is %d\n", response.c_str(), response.size());
                if (!response.empty())
                {
                    _sendBuffer->setBuf(response.c_str(), response.size());
                    _channel->enableWriting(); // 数据数据完成，开启写事件
                    printf("业务处理完成，开启写事件\n");
                }
                else
                {
                    printf("等待更多事件\n");
                }
            }
            else
            { // 没有设置业务处理回调的话使用默认处理
                printf("启动默认处理业务\n");
                echo(_sock->getfd());
            }
            _readBuffer->clear();
            // printf("finish reading once, errno: %d\n", errno); //11
            break;
        }
        else
        {
            printf("Connection reset by peer\n");
            _loop->deleteChannel(_channel);
            _loop->queueInloop([this]()
                               { _deleteConnectionCallback(_sock); }); // 添加进缓删队列
            break;
        }
    }
}

void Connection::echo(int sockfd)
{
    printf("message from client fd %d: %s\n", sockfd, _readBuffer->c_str());
    _sendBuffer->setBuf(_readBuffer->c_str(), _readBuffer->size());
    _channel->enableReading(); // 统一使用handlerWrite
    _readBuffer->clear();
}

void Connection::setDeleteConnectionCallback(std::function<void(Socket *)> cb)
{
    _deleteConnectionCallback = cb;
}

void Connection::setMessageCallback(std::function<std::string(
                                        int clnt_sock,
                                        const std::string &clnt_message,
                                        std::unordered_map<int, const Connection *> &userConnection,
                                        const Connection *currentConnection)>
                                        callback)
{
    _messageCallback = callback;
}

void Connection::handlerWrite()
{
    if (_sendBuffer->empty())
    {
        _channel->disableWriting(); // 没有数据就关闭写事件
        return;
    }
    while (!_sendBuffer->empty())
    {
        ssize_t bytes_write = write(_sock->getfd(),
                                    _sendBuffer->c_str(),
                                    _sendBuffer->size());
        if (bytes_write > 0)
        {
            // 成功写了byte_write字节，从缓冲区头部删除
            _sendBuffer->eraseFront(bytes_write);
        }
        else if (bytes_write == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            // 写缓冲区满，等待下次EPOLLOUT操作
            return; // 保持enableWriting状态
        }
        else
        {
            // 发生了真正的错误（连接断开等）
            printf("write error, client fd %d disconnected\n", _sock->getfd());
            _loop->deleteChannel(_channel);
            _loop->queueInloop([this]()
                               { _deleteConnectionCallback(_sock); });
            return;
        }
    }

    // 全部发送完毕
    _channel->disableWriting();
}

/*
    后面可以拓展一个功能
    通过调用这个连接的函数
    直接启动EPOLL的EPOLLOUT事件
    将函数的参数设置为string类型
    传递约定好的协议json格式string内容
    将数据发送给连接的客户端
*/