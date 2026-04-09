#include "Socket.h"
#include "InetAddress.h"
#include <fcntl.h>
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
    errif(_sockfd == -1, "socket create error");
}
Socket::Socket(int fd)
    : _sockfd(fd)
{
    errif(_sockfd == -1, "socket create error");
}
void Socket::bind(InetAddress *serv_addr)
{
    errif(::bind(_sockfd, (sockaddr *)&(serv_addr->_addr), serv_addr->_len) == -1, "socket bind error");
}

void Socket::listen()
{
    errif(::listen(_sockfd, SOMAXCONN) == -1, "socket listen error");
}

int Socket::accept(InetAddress *clnt_addr)
{
    int clnt_sockfd = ::accept(_sockfd, (sockaddr *)&(clnt_addr->_addr), &(clnt_addr->_len));
    errif(clnt_sockfd == -1, "socket accept new client error");
    return clnt_sockfd;
}

int Socket::getfd()
{
    return _sockfd;
}

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
}

void Socket::connect(InetAddress *addr)
{
    struct sockaddr_in _addr = addr->getAddr();
    errif(::connect(_sockfd, (sockaddr *)&_addr, sizeof(_addr)) == -1, "socket connect error");
}