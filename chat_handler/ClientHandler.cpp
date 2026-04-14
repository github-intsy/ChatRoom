#include "ClientHandler.h"
#include "json.hpp"
#include "ChatAccountHandler.h"
#include "Connection.h"
#include "protocol.h"
#include <iostream>
#include <arpa/inet.h>
ClientHandler::ClientHandler()
    : _logging(false), _accountHandler(nullptr)
{
    _accountHandler = new AccountHandler(_userConnections);
}

ClientHandler::~ClientHandler()
{
    if (_accountHandler != nullptr)
    {
        delete _accountHandler;
        _accountHandler = nullptr;
    }
}

std::string ClientHandler::ProcessClientMessage(int clnt_sock, const std::string &clnt_message,
                                                std::unordered_map<int, const Connection *> &userConnection,
                                                const Connection *currentConnection)
{
    if (clnt_message.size() < 4)
    {
        printf("数据长度不足，等待更多数据\n");
        return "";
    }
    // Qt QDataStream默认BigENdian，取出4字节长度
    uint32_t len = 0;
    std::memcpy(&len, clnt_message.data(), 4);

    len = ntohl(len); // 网络序列转化成本地序

    if (clnt_message.size() < 4 + len)
    {
        printf("收到部分消息,等待更多数据");
        return "";
    }
    std::string json_str = clnt_message.substr(4, len);
    try
    {
        auto j = nlohmann::json::parse(json_str);
        json_rpc_protocol::cmd_type cmd =
            static_cast<json_rpc_protocol::cmd_type>(j.value("cmd", 0));
        std::string ret;
        switch (cmd)
        {
        case json_rpc_protocol::cmd_type::CMD_REGISTER_REQ:
            ret = _accountHandler->handleRegister(j["params"]);
            break;
        case json_rpc_protocol::cmd_type::CMD_LOGIN_REQ:
            ret = _accountHandler->handleLogin(j["params"], userConnection, currentConnection);
            break;
        case json_rpc_protocol::cmd_type::CMD_RESET_PW_REQ:
            ret = _accountHandler->handleResetPassword(j["params"]);
            break;
        default:
            nlohmann::json response;
            response["cmd"] = json_rpc_protocol::cmd_type::STATUS_UNKNOWN_CMD;
            response["status"] = json_rpc_protocol::cmd_type::STATUS_UNKNOWN_CMD;
            response["message"] = "未知的错误cmd";
            break;
        }
        // 返回的格式必须一致
        uint32_t resp_len = ret.size();
        uint32_t be_len = htonl(resp_len);

        std::string full_response;
        full_response.append(reinterpret_cast<const char *>(&be_len), 4);
        full_response.append(ret);

        return full_response;
    }
    catch (const std::exception &e)
    {
        printf("JSON 解析失败：%s\n", e.what());
    }
    return "";
}