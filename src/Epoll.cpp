#include "Epoll.h"
#include "Logger.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/epoll.h>
#include <unistd.h>
Epoll::Epoll() : _epfd(-1), _events(nullptr)
{
    // 创建一个用于在内核存放数据的句柄
    _epfd = epoll_create1(0);
    if (_epfd == -1)
        {
            LOG_ERROR("Server epoll create error");
        }
    else
        LOG_INFO("Server epoll create success");
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

//     epoll_ctl(_epfd, EPOLL_CTL_ADD, sockfd, &ev); // 将客户端socket
//     fd添加到epoll
// }

std::vector<Channel *> Epoll::poll(int timeout)
{
    std::vector<Channel *> ret;
    int nfds = epoll_wait(_epfd, _events, MAX_EVENTS, timeout);
    if (nfds == -1)
    {
        std::string message = "Server epoll wait error, epoll fd is ";
        message += std::to_string(_epfd);
        LOG_ERROR(message.c_str());
    }
    else
    {
        std::string message = "Server epoll wait success, epoll fd is ";
        message += std::to_string(_epfd);
        LOG_INFO(message.c_str());
    }
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
        if (epoll_ctl(_epfd, EPOLL_CTL_ADD, fd, &ev) == -1) // 添加Channel中fd到epoll
        {
            std::string message("fd add epoll error, fd is ");
            message += std::to_string(fd);
            LOG_ERROR(message);
        }
        else
        {
            std::string message = "fd add epoll success, fd is " + std::to_string(fd);
            LOG_INFO(message);
        }
        channel->setInEpoll();
    }
    else
    {
        errif(epoll_ctl(_epfd, EPOLL_CTL_MOD, fd, &ev) == -1,
              "epoll modify error"); // 已存在，修改
    }
}

void Epoll::deleteChannel(Channel *channel)
{
    int fd = channel->getFd();
    errif(epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, nullptr) == -1,
          "epoll delete error");
    channel->setInEpoll(false);
}