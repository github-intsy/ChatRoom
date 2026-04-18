#include "Connection.h"
#include "Buffer.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Socket.h"
#include "Logger.h"
#include "util.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <functional>
#include <mutex>
#include <unistd.h>

#define READ_BUF 1024

Connection::Connection(EventLoop *loop, Socket *sock, std::unordered_map<int, Connection *> *userConnections)
    : _sock(sock), _loop(loop), _channel(nullptr), _sendBuffer(new Buffer()),
      _readBuffer(new Buffer()), _userConnections(userConnections)
{
    _channel = new Channel(_loop, _sock->getfd());
    std::function<void()> rcb = std::bind(&Connection::handleRead, this, _sock->getfd());
    std::function<void()> wcb = std::bind(&Connection::handlerWrite, this);
    _channel->setReadCallback(rcb);
    _channel->setWriteCallback(wcb);
    LOG_INFO("connection set call back function");
    _channel->enableReading();
    _channel->useET();
    LOG_INFO("connection enable reading and use ET mode");
}

Connection::~Connection()
{
    delete _channel;
    delete _sock;
    delete _readBuffer;
    delete _sendBuffer;
}

void Connection::handleRead(int sockfd)
{
    char buf[READ_BUF];

    while (true)
    {
        bzero(buf, sizeof(buf));
        ssize_t read_bytes = read(sockfd, buf, sizeof(buf));

        if (read_bytes > 0)
        {
            _readBuffer->append(buf, read_bytes);
            // std::string message = "Data is: " + _readBuffer->getBuffer() +
            //                       ", Size is: " + std::to_string(read_bytes);
            // LOG_DEBUG(message);
        }
        else if (read_bytes == 0)
        {
            std::string message = "EOF, client fd " + std::to_string(sockfd) + " disconnected";
            LOG_INFO(message);
            _loop->deleteChannel(_channel);
            _loop->queueInloop([this]()
                               { _deleteConnectionCallback(_sock); });
            break;
        }
        else if (read_bytes == -1 && errno == EINTR)
        {
            LOG_INFO("Continue reading");
            continue;
        }
        else if (read_bytes == -1 && ((errno == EAGAIN) || errno == EWOULDBLOCK))
        {
            if (_messageCallback)
            {
                LOG_DEBUG("Start transaction processing");

                while (true)
                {
                    if (_readBuffer->size() < 4)
                        break;

                    uint32_t len = 0;
                    memcpy(&len, _readBuffer->c_str(), 4);
                    len = ntohl(len);

                    if (_readBuffer->size() < static_cast<ssize_t>(4 + len))
                        break;

                    std::string onePacket(_readBuffer->c_str() + 4, len);
                    _readBuffer->eraseFront(4 + len);

                    std::string response = _messageCallback(onePacket, this, _userConnections);

                    std::string info = "Response is: " + response +
                                       ", size is " + std::to_string(response.size());
                    LOG_DEBUG(info);

                    if (!response.empty())
                    {
                        std::string framed = encodePacket(response);
                        _sendBuffer->append(framed.c_str(), framed.size());
                        _channel->enableWriting();
                        LOG_DEBUG("Write event enabled");
                    }
                }
            }
            else
            {
                LOG_INFO("Start default processing");
                echo(_sock->getfd());
            }
            break;
        }
        else
        {
            LOG_INFO("Connection reset by peer");
            _loop->deleteChannel(_channel);
            _loop->queueInloop([this]()
                               { _deleteConnectionCallback(_sock); });
            break;
        }
    }
}

void Connection::echo(int sockfd)
{
    std::string info = "message from client fd " + std::to_string(sockfd) + ": " + _readBuffer->getBuffer();
    LOG_INFO(info);

    std::string framed = encodePacket(_readBuffer->getBuffer());
    _sendBuffer->setBuf(framed.c_str(), framed.size());
    _channel->enableWriting();
    _readBuffer->clear();
}

void Connection::setDeleteConnectionCallback(std::function<void(Socket *)> cb)
{
    _deleteConnectionCallback = cb;
}

void Connection::setMessageCallback(std::function<std::string(
                                        const std::string &clnt_message, Connection *currentConnection,
                                        std::unordered_map<int, Connection *> *userConnection)>
                                        callback)
{
    _messageCallback = callback;
}

void Connection::handlerWrite()
{
    std::string info;
    if (_sendBuffer->empty())
    {
        LOG_DEBUG("sendBuffer is empty, close writing");
        _channel->disableWriting();
        return;
    }

    while (!_sendBuffer->empty())
    {
        ssize_t bytes_write = write(_sock->getfd(),
                                    _sendBuffer->c_str(),
                                    _sendBuffer->size());
        if (bytes_write > 0)
        {
            info = "write " + std::to_string(bytes_write) + " bytes";
            LOG_DEBUG(info);
            _sendBuffer->eraseFront(bytes_write);
        }
        else if (bytes_write == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            LOG_INFO("Buffer overflow detected. Waiting for the next EPOLLOUT operation");
            return;
        }
        else
        {
            info = "write error, client fd " + std::to_string(_sock->getfd()) + " disconnected";
            LOG_WARN(info);
            _loop->deleteChannel(_channel);
            _loop->queueInloop([this]()
                               { _deleteConnectionCallback(_sock); });
            return;
        }
    }

    _channel->disableWriting();
    LOG_INFO("All responses are send");
}

void Connection::send(const std::string &message)
{
    if (message.empty())
        return;

    LOG_INFO("send message is: " + message);
    std::unique_lock<std::mutex> lock(_sendMutex);

    std::string framed = encodePacket(message);
    _sendBuffer->append(framed.c_str(), framed.size());

    if (_channel)
        _channel->enableWriting();
}

void Connection::setUserId(int userId)
{
    _userId = userId;
}

int Connection::getUserId() const
{
    return _userId;
}