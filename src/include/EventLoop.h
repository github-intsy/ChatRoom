#pragma once
#include <functional>
#include <vector>
class Epoll;
class Channel;

class EventLoop
{
private:
    Epoll *_ep;
    bool _quit;
    std::vector<std::function<void()>> _pendingFunctors; // 延迟删除的队列
public:
    EventLoop();
    ~EventLoop();
    void loop();
    void doPendingFunctors();                   // 执行删除动作
    void queueInloop(std::function<void()> cb); // 增加任务到删除队列统一删除
    void updateChannel(Channel *);
    void deleteChannel(Channel *channel);
};