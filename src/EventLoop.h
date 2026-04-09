#pragma once
#include <functional>
class Epoll;
class Channel;
class ThreadPool;

class EventLoop
{
private:
    Epoll *_ep;
    ThreadPool *_threadPool;
    bool _quit;

public:
    EventLoop();
    ~EventLoop();
    void loop();
    void updateChannel(Channel *);
    void addThread(std::function<void()>);
};