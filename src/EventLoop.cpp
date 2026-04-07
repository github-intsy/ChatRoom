#include"EventLoop.h"
#include "Epoll.h"
#include "Channel.h"
#include <vector>

EventLoop::EventLoop() :_ep(nullptr), _quit(false)
{
    _ep = new Epoll();
}

EventLoop::~EventLoop()
{
    delete _ep;
}

void EventLoop::loop()
{
    while(!_quit)
    {
        std::vector<Channel*> chs;
        chs = _ep->poll();
        for(auto it = chs.begin(); it != chs.end(); ++it)
        {
            (*it)->handleEvent();
        }
    }
}

void EventLoop::updateChannel(Channel* c)
{
    _ep->updateChannel(c);//更新事件在events上面的状态和socket与epoll的绑定状态
}