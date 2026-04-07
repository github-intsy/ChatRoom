#pragma once
#include <vector>
#include "Channel.h"
class Channel;
class Epoll
{
public:
    Epoll();
    ~Epoll();
    void addFd(int, uint32_t);
    std::vector<Channel *> poll(int timeout = -1);
    void updateChannel(Channel *channel);

private:
    // events是存放事件的数组，ev是存放事件的模板
    static const int MAX_EVENTS = 100; // 定义最大监听连接数量
    int _epfd;
    struct epoll_event *_events;
};