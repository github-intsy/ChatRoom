#pragma once
#include <cstdio>
#include <cstdlib>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string>
#include <cstring>
class InetAddress
{
public:
    InetAddress(const std::string& ip
    , const unsigned char port
    , unsigned short int sin_family=AF_INET);
    struct sockaddr_in _addr;
    socklen_t _len;
};