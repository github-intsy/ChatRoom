#ifndef CHAT_MESSAGE_HANDLER_H
#define CHAT_MESSAGE_HANDLER_H

#include <string>
#include <unordered_map>
#include "json.hpp"
#include "protocol.h"

class Connection;
class ConnectionPool;

class MessageHandler
{
public:
    explicit MessageHandler(std::unordered_map<int, Connection *> *userConnections = nullptr);

    std::string handleSendPrivateMessage(nlohmann::json j,
                                         int senderId,
                                         std::unordered_map<int, Connection *> *currentConnection);

    std::string handleGetPrivateHistory(nlohmann::json j, int userId);

    std::string handleSendGroupMessage(nlohmann::json j,
                                       int senderId,
                                       std::unordered_map<int, Connection *> *currentConnection);

    std::string handleGetGroupHistory(nlohmann::json j, int userId);

private:
    std::string retMessage(json_rpc_protocol::cmd_type cmd,
                           json_rpc_protocol::cmd_type status,
                           const char *message);

    std::string retMessageWithData(json_rpc_protocol::cmd_type cmd,
                                   json_rpc_protocol::cmd_type status,
                                   const char *message,
                                   const nlohmann::json &data);

private:
    ConnectionPool *_cpool;
    std::unordered_map<int, Connection *> *_userConnections;
};

#endif