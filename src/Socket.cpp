#include "Socket.h"
#include "InetAddress.h"
#include "Logger.h"
#include <fcntl.h>
#include <errno.h>
#include <cstring>
// int epfd = epoll_create(0);//创建一个epoll文件描述符并返回，失败是-1
// errif(epfd==-1, "server epoll create error");
/*
1. AF_INET ipv4
2. SOCK_STREAM TCP
2. SOCK_DGRAM UDP
3. 0根据前面两个参数推导
3. IPPROTO_TCP TCP
3. IPTOTO_UDP udp
创建一个套接字的文件描述符
*/
Socket::Socket()
{
    _sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (_sockfd == -1)
    {
        LOG_ERROR("socket create error");
    }
    else
        LOG_INFO("socket create success");
}
Socket::Socket(int fd) : _sockfd(fd)
{
    if (_sockfd == -1)
    {
        LOG_ERROR("socket create error");
    }
    else
        LOG_INFO("socket create success");
}
void Socket::bind(InetAddress *serv_addr)
{
    if (::bind(_sockfd, (sockaddr *)&(serv_addr->_addr), serv_addr->_len) == -1)
    {
        std::string err = "socket bind error, " + std::string(strerror(errno));
        LOG_ERROR(err);
    }
    else
        LOG_INFO("socket bind success");
}

void Socket::listen()
{
    if (::listen(_sockfd, SOMAXCONN) == -1)
    {
        LOG_ERROR("socket listen error");
    }
    else
        LOG_INFO("socket listen success");
}

int Socket::accept(InetAddress *clnt_addr)
{
    int clnt_sockfd =
        ::accept(_sockfd, (sockaddr *)&(clnt_addr->_addr), &(clnt_addr->_len));
    if (clnt_sockfd == -1)
    {
        LOG_ERROR("socket accept new client error");
    }
    else
        LOG_INFO("socket accept a new client connection");
    return clnt_sockfd;
}

int Socket::getfd() { return _sockfd; }

Socket::~Socket()
{
    if (_sockfd != -1)
    {
        close(_sockfd);
        _sockfd = -1;
    }
}

void Socket::setnonblocking()
{
    fcntl(_sockfd, F_SETFL, fcntl(_sockfd, F_GETFL) | O_NONBLOCK);
    std::string message = "Set the fd: " + std::to_string(_sockfd) + " nonblocking";
    LOG_INFO(message);
}

void Socket::connect(InetAddress *addr)
{
    struct sockaddr_in _addr = addr->getAddr();
    if (::connect(_sockfd, (sockaddr *)&_addr, sizeof(_addr)) == -1)
    {
        LOG_ERROR("socket connect error");
    }
    else
        LOG_INFO("socket connect success");
}