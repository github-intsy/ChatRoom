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
#include "ConnectionPool.h"
#include "ChatAccountHandler.h"

#define READ_BUF 1024

Server::Server(EventLoop *loop) : _mainReactor(loop), _acceptor(nullptr)
{
	_acceptor = new Acceptor(_mainReactor);
	LOG_INFO("Server start listen new connections...");

	_client = new ClientHandler();

	std::function<void(Socket *)> cb =
		std::bind(&Server::newConnection, this, std::placeholders::_1);
	_acceptor->setNewConnectionCallback(cb);

	int size = std::thread::hardware_concurrency();
	if (size <= 0)
		size = 4;

	_thpool = new ThreadPool(size);
	LOG_INFO("Server thread pool is start");

	for (int i = 0; i < size; ++i)
	{
		_subReactors.push_back(new EventLoop());
	}

	for (int i = 0; i < size; ++i)
	{
		std::function<void()> sub_loop =
			std::bind(&EventLoop::loop, _subReactors[i]);
		_thpool->add(sub_loop);
	}

	LOG_INFO("Server event loop is enable");
}

Server::~Server()
{
	delete _acceptor;
}

void Server::newConnection(Socket *sock)
{
	uint64_t random = sock->getfd() % _subReactors.size();
	Connection *conn = new Connection(_subReactors[random], sock, &_userConnections);
	LOG_INFO("server accept a new connection");

	std::function<std::string(const std::string &, Connection *, std::unordered_map<int, Connection *> *)>
		processCallback = std::bind(&ClientHandler::handleRequest,
									_client,
									std::placeholders::_1,
									std::placeholders::_2,
									std::placeholders::_3);

	conn->setMessageCallback(processCallback);

	std::function<void(Socket *)> cb =
		std::bind(&Server::deleteConnection, this, std::placeholders::_1);
	conn->setDeleteConnectionCallback(cb);

	_connections[sock->getfd()] = conn;
}

void Server::deleteConnection(Socket *sock)
{
	if (sock->getfd() == -1)
		return;

	auto it = _connections.find(sock->getfd());
	if (it == _connections.end())
		return;

	Connection *conn = it->second;
	_connections.erase(sock->getfd());

	int userId = conn->getUserId();
	if (userId > 0)
	{
		auto db = ConnectionPool::getConnectionPool()->getConnection();
		if (db)
		{
			long long msgId = 0;
			std::string sql = "update sys_user set status = 0 where user_id = " + std::to_string(userId);
			db->update(sql, msgId);
		}

		_userConnections.erase(userId);

		AccountHandler handler(&_userConnections);
		handler.notifyFriendOnlineStatus(userId, 0);
	}

	delete conn;
	LOG_INFO("Server releases a connection");
}