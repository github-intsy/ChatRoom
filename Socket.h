#pragma once
#include <cstdio>
#include <cstdlib>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include "util.h"
#include "InetAddress.h"
class Socket
{
public:
    Socket();
    void bind(InetAddress* serv_addr);
    void listen();
    int accept(InetAddress* clnt_addr);
    int getfd();
private:
    int _sockfd;
};