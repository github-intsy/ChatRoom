#pragma once
#include <map>
#include <vector>
#include <unordered_map>
class EventLoop;
class Socket;
class Acceptor;
class Connection;
class ThreadPool;
class ClientHandler;

class Server
{
private:
  Acceptor *_acceptor;                                          // 连接接收器
  std::map<int, Connection *> _connections;                     // 所有tcp连接
  EventLoop *_mainReactor;                                      // 只负责接受连接，然后分发给一个sub Reactor
  std::vector<EventLoop *> _subReactors;                        // 负责处理事件循环
  ThreadPool *_thpool;                                          // 线程池
  ClientHandler *_client;                                       // 业务处理中间层
  std::unordered_map<int, const Connection *> _userConnections; // 维护用户登录状态，指针为空表示离线
public:
  Server(EventLoop *);
  ~Server();
  void newConnection(Socket *sock);    // 新建TCP连接
  void deleteConnection(Socket *sock); // 断开TCP连接
};