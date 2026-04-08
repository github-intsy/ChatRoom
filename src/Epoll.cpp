#include "Epoll.h"
#include <cstdio>
#include <cstdlib>
#include <sys/epoll.h>
#include <cstring>
#include <unistd.h>
#include "util.h"
Epoll::Epoll()
    : _epfd(-1), _events(nullptr)
{
    // 创建一个用于在内核存放数据的句柄
    _epfd = epoll_create1(0);
    errif(_epfd == -1, "epoll create error");
    _events = new struct epoll_event[MAX_EVENTS];
    bzero(_events, sizeof(*_events) * MAX_EVENTS);
}

Epoll::~Epoll()
{
    if (_epfd != -1)
    {
        close(_epfd);
        _epfd = -1;
    }
    delete[] _events;
}

// void Epoll::addFd(int sockfd, uint32_t events)
// {
//     struct epoll_event ev; // 设置样板参数
//     ev.data.fd = sockfd;
//     ev.events = events;

//     epoll_ctl(_epfd, EPOLL_CTL_ADD, sockfd, &ev); // 将客户端socket fd添加到epoll
// }

std::vector<Channel *> Epoll::poll(int timeout)
{
    std::vector<Channel *> ret;
    int nfds = epoll_wait(_epfd, _events, MAX_EVENTS, timeout);
    errif(nfds == -1, "epoll wait error");
    for (int i = 0; i < nfds; ++i)
    {
        Channel *ch = (Channel *)_events[i].data.ptr;
        ch->setRevents(_events[i].events);
        ret.push_back(ch);
    }
    return ret;
}

void Epoll::updateChannel(Channel *channel)
{
    int fd = channel->getFd();
    struct epoll_event ev;
    bzero(&ev, sizeof(ev));
    ev.data.ptr = channel;
    ev.events = channel->getEvents();
    if (!channel->getInEpoll())
    {
        errif(epoll_ctl(_epfd, EPOLL_CTL_ADD, fd, &ev) == -1, "epoll add error"); // 添加Channel中fd到epoll
        channel->setInEpoll();
    }
    else
    {
        errif(epoll_ctl(_epfd, EPOLL_CTL_MOD, fd, &ev) == -1, "epoll modify error"); // 已存在，修改
    }
}