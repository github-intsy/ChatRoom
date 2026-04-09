#include "EventLoop.h"
#include "Epoll.h"
#include "Channel.h"
#include <vector>
#include "ThreadPool.h"
EventLoop::EventLoop() : _ep(nullptr), _quit(false)
{
    _ep = new Epoll();
    _threadPool = new ThreadPool(); // 初始化线程池
}

EventLoop::~EventLoop()
{
    delete _ep;
}

void EventLoop::loop()
{
    while (!_quit)
    {
        std::vector<Channel *> chs;
        chs = _ep->poll();
        for (auto it = chs.begin(); it != chs.end(); ++it)
        {
            (*it)->handleEvent();
        }
    }
}

void EventLoop::updateChannel(Channel *c)
{
    _ep->updateChannel(c); // 更新事件在events上面的状态和socket与epoll的绑定状态
}

void EventLoop::addThread(std::function<void()> func)
{
    _threadPool->add(func);
}