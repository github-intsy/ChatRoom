#pragma once
#include "util.h"
#include <arpa/inet.h>
#include <cstdio>
#include <cstdlib>
#include <sys/epoll.h>
#include <unistd.h>
class InetAddress;
class Socket {
public:
  Socket();
  Socket(int sockfd);
  void bind(InetAddress *serv_addr);
  void listen();
  int accept(InetAddress *clnt_addr);
  int getfd();
  void setnonblocking();
  ~Socket();
  void connect(InetAddress *addr);

private:
  int _sockfd;
};