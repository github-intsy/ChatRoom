#include "Channel.h"
#include "EventLoop.h"
#include <unistd.h>
Channel::Channel(EventLoop *loop, int fd)
    : _loop(loop), _fd(fd), _events(0), _revents(0), _inEpoll(false) {}

Channel::~Channel() {
  if (_fd != -1) {
    close(_fd);
    _fd = -1;
  }
}

void Channel::enableReading() {
  _events = EPOLLIN | EPOLLPRI;
  _loop->updateChannel(this);
}

int Channel::getFd() { return _fd; }

uint32_t Channel::getEvents() { return _events; }

bool Channel::getInEpoll() { return _inEpoll; }

void Channel::setInEpoll(bool flag) { _inEpoll = flag; }

void Channel::setRevents(const uint32_t revents) { _revents = revents; }

void Channel::setReadCallback(std::function<void()> cb) { _readCallback = cb; }

void Channel::handleEvent() {
  // _callback();
  // _loop->addThread(_callback);
  if (_revents & (EPOLLIN | EPOLLPRI)) {
    _readCallback();
  }
  if (_revents & (EPOLLOUT)) {
    _writeCallback();
  }
}

void Channel::useET() {
  _events |= EPOLLET;
  _loop->updateChannel(this);
}

uint32_t Channel::getRevent() { return _revents; }