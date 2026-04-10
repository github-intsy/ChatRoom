#pragma once
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/socket.h>
class InetAddress {
public:
  InetAddress(const std::string &ip, const uint16_t port,
              unsigned short int sin_family = AF_INET);
  InetAddress();

  sockaddr_in getAddr();

  struct sockaddr_in _addr;
  socklen_t _len;
};