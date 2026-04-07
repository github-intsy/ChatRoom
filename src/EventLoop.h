#pragma once

class Epoll;
class Channel;

class EventLoop
{
private:
    Epoll *_ep;
    bool _quit;
public:
    EventLoop();
    ~EventLoop();
    void loop();
    void updateChannel(Channel*);
};