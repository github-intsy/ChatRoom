#include "ClientHandler.h"
#include "ChatAccountHandler.h"
#include "ChatMessageHandler.h"
#include "Connection.h"
#include "protocol.h"

ClientHandler::ClientHandler()
{
    _accountHandler = new AccountHandler();
    _messageHandler = new MessageHandler();
}

ClientHandler::~ClientHandler()
{
    delete _accountHandler;
    delete _messageHandler;
}

std::string ClientHandler::handleRequest(const std::string &request,
                                         Connection *connection,
                                         std::unordered_map<int, Connection *> *userConnections)
{
    using json_rpc_protocol::cmd_type;

    try
    {
        nlohmann::json req = nlohmann::json::parse(request);
        int cmd = req.value("cmd", 0);
        nlohmann::json params = req.value("params", nlohmann::json::object());

        int userId = connection ? connection->getUserId() : 0;

        switch (cmd)
        {
        case static_cast<int>(cmd_type::CMD_LOGIN_REQ):
        {
            int loginUserId = 0;
            return _accountHandler->handleLogin(params, userConnections, connection, loginUserId);
        }
        case static_cast<int>(cmd_type::CMD_RESET_PW_REQ):
            return _accountHandler->handleResetPassword(params);
        case static_cast<int>(cmd_type::CMD_REGISTER_REQ):
            return _accountHandler->handleRegister(params);

        case static_cast<int>(cmd_type::CMD_UPDATE_AVATAR_REQ):
            return _accountHandler->handleUpdateAvatar(params, userConnections, userId);

        case static_cast<int>(cmd_type::CMD_CREATE_GROUP_REQ):
            return _accountHandler->handleCreateGroup(params, userId);

        case static_cast<int>(cmd_type::CMD_JOIN_GROUP_REQ):
            return _accountHandler->handleJoinGroup(params, userId);

        case static_cast<int>(cmd_type::CMD_GROUP_MANAGE_REQ):
            return _accountHandler->handleGroupManage(params, userId);

        case static_cast<int>(cmd_type::CMD_SEND_PRIVATE_MSG_REQ):
            return _messageHandler->handleSendPrivateMessage(params, userId, userConnections);

        case static_cast<int>(cmd_type::CMD_GET_PRIVATE_HISTORY_REQ):
            return _messageHandler->handleGetPrivateHistory(params, userId);

        case static_cast<int>(cmd_type::CMD_SEND_GROUP_MSG_REQ):
            return _messageHandler->handleSendGroupMessage(params, userId, userConnections);

        case static_cast<int>(cmd_type::CMD_GET_GROUP_HISTORY_REQ):
            return _messageHandler->handleGetGroupHistory(params, userId);

        case static_cast<int>(cmd_type::CMD_SEARCH_USER_REQ):
            return _accountHandler->handleSearchUser(params, userId);

        case static_cast<int>(cmd_type::CMD_ADD_FRIEND_REQ):
            return _accountHandler->handleAddFriend(params, userId);

        case static_cast<int>(cmd_type::CMD_GET_FRIEND_REQUEST_LIST_REQ):
            return _accountHandler->handleGetFriendRequestList(userId);

        case static_cast<int>(cmd_type::CMD_HANDLE_FRIEND_REQUEST_REQ):
            return _accountHandler->handleFriendRequest(params, userId);

        case static_cast<int>(cmd_type::CMD_INVITE_GROUP_MEMBER_REQ):
            return _accountHandler->handleInviteGroupMember(params, userId);

        case static_cast<int>(cmd_type::CMD_GET_GROUP_MEMBERS_REQ):
            return _accountHandler->handleGetGroupMembers(params, userId);
        case static_cast<int>(cmd_type::CMD_DELETE_FRIEND_REQ):
            return _accountHandler->handleDeleteFriend(params, userId);
        default:
        {
            nlohmann::json res;
            res["cmd"] = 0;
            res["status"] = static_cast<int>(cmd_type::STATUS_UNKNOWN_CMD);
            res["message"] = "未知命令";
            return res.dump();
        }
        }
    }
    catch (const std::exception &e)
    {
        nlohmann::json res;
        res["cmd"] = 0;
        res["status"] = static_cast<int>(json_rpc_protocol::cmd_type::STATUS_FAILED);
        res["message"] = e.what();
        return res.dump();
    }
}