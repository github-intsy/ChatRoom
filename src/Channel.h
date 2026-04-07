#pragma once
#include <functional>
#include <sys/epoll.h>
// 对于每个event事件指向Channel类，每个Channel唯一对应一个fd，对其不同的事件类型做不同的处理
class EventLoop;

class Channel
{
public:
    Channel(EventLoop *loop, int fd);
    void enableReading();
    int getFd();
    uint32_t getEvents();
    bool getInEpoll();
    void setInEpoll();
    void setRevents(const uint32_t events);
    void setCallback(std::function<void()>);
    void handleEvent();

private:
    EventLoop *_loop;  // 每个socket都会被分配到一个epoll类
    int _fd;           // socketfd唯一对应一个Channel类
    uint32_t _events;  // 监听这个文件描述符对应哪个事件
    uint32_t _revents; // 表示epoll返回该channel时fd正在发生的事件
    bool _inEpoll;     // 当前fd是否在epoll红黑树中
    std::function<void()> _callback;
};