#include "InetAddress.h"
InetAddress::InetAddress(const std::string &ip, const unsigned char port, unsigned short int sin_family)
{
    // 初始化结构体
    bzero(&_addr, sizeof(_addr));
    // 设置socket参数
    _addr.sin_family = sin_family;
    _addr.sin_addr.s_addr = inet_addr(ip.c_str());
    _addr.sin_port = htons(port);
    _len = sizeof(_addr);
}