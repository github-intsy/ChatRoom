#include "Channel.h"
#include "EventLoop.h"
#include <unistd.h>
Channel::Channel(EventLoop *loop, int fd)
    : _loop(loop), _fd(fd), _events(0), _revents(0), _inEpoll(false), _useThreadPool(true)
{
}

Channel::~Channel()
{
    if (_fd != -1)
    {
        close(_fd);
        _fd = -1;
    }
}

void Channel::enableReading()
{
    _events = EPOLLIN | EPOLLPRI;
    _loop->updateChannel(this);
}

int Channel::getFd()
{
    return _fd;
}

uint32_t Channel::getEvents()
{
    return _events;
}

bool Channel::getInEpoll()
{
    return _inEpoll;
}

void Channel::setInEpoll()
{
    _inEpoll = true;
}

void Channel::setRevents(const uint32_t revents)
{
    _revents = revents;
}

void Channel::setCallback(std::function<void()> cb)
{
    _callback = cb;
}

void Channel::handleEvent()
{
    // _callback();
    // _loop->addThread(_callback);
    if(_revents & (EPOLLIN | EPOLLPRI))
    {
        if(_useThreadPool)
            _loop->addThread(_callback);
        else
            _callback();
    }
    if(_revents & (EPOLLOUT))
    {
        if(_useThreadPool)
    }
}

void Channel::useET()
{
    _events |= EPOLLET;
    _loop->updateChannel(this);
}