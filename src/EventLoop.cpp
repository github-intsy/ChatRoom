#include "EventLoop.h"
#include "Channel.h"
#include "Epoll.h"
#include "Logger.h"
EventLoop::EventLoop() : _ep(nullptr), _quit(false) { _ep = new Epoll(); }

EventLoop::~EventLoop() { delete _ep; }

void EventLoop::loop()
{
    while (!_quit)
    {
        std::vector<Channel *> chs;
        chs = _ep->poll();
        for (auto it = chs.begin(); it != chs.end(); ++it)
        {
            std::string message = "server processes a transaction, socket is ";
            message += (*it)->getFd();
            LOG_INFO(message.c_str());
            (*it)->handleEvent();
        }
        // 需要调用延迟删除
        doPendingFunctors();
    }
}

void EventLoop::updateChannel(Channel *c)
{
    _ep->updateChannel(c); // 更新事件在events上面的状态和socket与epoll的绑定状态
}

void EventLoop::deleteChannel(Channel *c)
{
    _ep->deleteChannel(c);
}

// 销毁维护连接对象
void EventLoop::doPendingFunctors()
{
    std::vector<std::function<void()>> funcs;
    funcs.swap(_pendingFunctors); // 避免递归死锁
    for (auto &f : funcs)
    {
        f();
    }
}

// 增加待删除的事件，统一删除
void EventLoop::queueInloop(std::function<void()> cb)
{
    _pendingFunctors.push_back(cb);
}