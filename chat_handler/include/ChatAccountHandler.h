#pragma once
#include "json.hpp"
#include <string>
#include "protocol.h"
#include <unordered_map>
class ConnectionPool; // mysql连接池
class Connection;     // TCP连接
class AccountHandler
{
public:
    AccountHandler(std::unordered_map<int, Connection *> &userConnections);
    std::string handleRegister(nlohmann::json_abi_v3_12_0::json);
    std::string handleLogin(nlohmann::json_abi_v3_12_0::json, std::unordered_map<int, const Connection *> &, const Connection *);
    std::string handleResetPassword(nlohmann::json_abi_v3_12_0::json);

private:
    ConnectionPool *_cpool;
    std::unordered_map<int, Connection *> _userConnections;
    std::string retMessage(json_rpc_protocol::cmd_type cmd,
                           json_rpc_protocol::cmd_type status, const char *message);
};