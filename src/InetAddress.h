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
    InetAddress(const std::string &ip, const uint16_t port, unsigned short int sin_family = AF_INET);
    InetAddress();

    sockaddr_in getAddr();

    struct sockaddr_in _addr;
    socklen_t _len;
};