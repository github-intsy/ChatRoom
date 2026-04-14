#include "Server.h"
#include "Acceptor.h"
#include "Connection.h"
#include "EventLoop.h"
#include "Socket.h"
#include "ThreadPool.h"
#include <functional>
#include <thread>
#include "ClientHandler.h"
#include "Logger.h"
#define READ_BUF 1024

// 通过构造函数绑定Channel类和server的newConnection函数
Server::Server(EventLoop *loop) : _mainReactor(loop), _acceptor(nullptr)
{
	_acceptor = new Acceptor(_mainReactor); // 创建接收对象
	LOG_INFO("Server start listen new connections...");
	_client = new ClientHandler();
	std::function<void(Socket *)> cb =
		std::bind(&Server::newConnection, this, std::placeholders::_1);
	_acceptor->setNewConnectionCallback(cb);
	int size = std::thread::
		hardware_concurrency();		// 当前CPU核心数，也就是线程数，同时是subReactor数量
	_thpool = new ThreadPool(size); // 新建线程池
	LOG_INFO("Server thread pool is start");
	for (int i = 0; i < size; ++i)
	{
		_subReactors.push_back(new EventLoop()); // 为每个线程创建一个EventLoop
	}
	for (int i = 0; i < size; ++i)
	{
		std::function<void()> sub_loop =
			std::bind(&EventLoop::loop, _subReactors[i]);
		_thpool->add(sub_loop); // 开启所有线程的事件循环
	}
	LOG_INFO("Server event loop is enable");
}

Server::~Server() { delete _acceptor; }

// 创建客户端连接，将客户端连接加入map
void Server::newConnection(Socket *sock)
{
	uint64_t random = sock->getfd() % _subReactors.size();							  // 调度策略：全随机
	Connection *conn = new Connection(_subReactors[random], sock, &_userConnections); // 随机给这个sock分配一个subReactor
	LOG_INFO("server accept a new connection");
	std::function<std::string(int, const std::string &, std::unordered_map<int, const Connection *> &, const Connection *)>
		processCallback = std::bind(&ClientHandler::ProcessClientMessage,
									_client, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4);
	conn->setMessageCallback(processCallback);
	std::function<void(Socket *)> cb = std::bind(&Server::deleteConnection, this, std::placeholders::_1);
	conn->setDeleteConnectionCallback(cb);
	_connections[sock->getfd()] = conn;
}

//
void Server::deleteConnection(Socket *sock)
{
	if (sock->getfd() != -1)
	{
		auto it = _connections.find(sock->getfd());
		if (it != _connections.end())
		{
			Connection *conn = _connections[sock->getfd()];
			_connections.erase(sock->getfd());
			delete conn;
			LOG_INFO("Server releases a connection");
		}
	}
}
