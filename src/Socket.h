#pragma once
#include <cstdio>
#include <cstdlib>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "util.h"
#include "InetAddress.h"
class InetAddress;
class Socket
{
public:
    Socket();
    Socket(int sockfd);
    void bind(InetAddress *serv_addr);
    void listen();
    int accept(InetAddress *clnt_addr);
    int getfd();
    void setnonblocking();
    ~Socket();

private:
    int _sockfd;
};