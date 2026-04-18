#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include <unordered_map>
#include <string>
#include "json.hpp"
class Connection;
class AccountHandler;
class MessageHandler;

class ClientHandler
{
public:
    ClientHandler();
    ~ClientHandler();

    std::string handleRequest(const std::string& request,
                              Connection* connection,
                              std::unordered_map<int, Connection*> *userConnections);

private:
    AccountHandler* _accountHandler;
    MessageHandler* _messageHandler;
};

#endif