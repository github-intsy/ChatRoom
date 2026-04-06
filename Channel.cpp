#include"Channel.h"
Channel::Channel(Epoll *ep, int fd)
    : _ep(ep), _fd(fd), _events(0), _revents(0), _inEpoll(false)
{
    
}
void Channel::enableReading()
{
    _events = EPOLLIN | EPOLLET;
    _ep->updateChannel(this);
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
    _inEpoll=true;
}

void Channel::setRevents(const uint32_t events)
{
    _events = events;
}