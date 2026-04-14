#pragma once
#include <sys/socket.h>
#include <string>
#include <unordered_map>
/*
    处理中间层，由于Connection连接层只能绑定一个回调函数，
    通过绑定ClientHandler(本类)进行处理
    通过登录状态进行处理，如果未登录就调用ChatAccountHandler成员方法
    如果登录就调用ChatMessgaeHandler成员方法
    将CLinetHandler类作为一个事务分离和事务转发的一个中间类
*/

class AccountHandler;
class Connection;

class ClientHandler
{
public:
    ClientHandler();
    ~ClientHandler();
    // 消息处理中间层函数，分解消息根据类型进行事务的派发
    std::string ProcessClientMessage(int clnt_sock, const std::string &clnt_message, std::unordered_map<int, const Connection *> &, const Connection *);

private:
    bool _logging;                                          // 判断登录状态
    AccountHandler *_accountHandler;                        // 维护登录事务处理方法
    std::unordered_map<int, Connection *> _userConnections; // 维护用户状态（在线/离线）
};