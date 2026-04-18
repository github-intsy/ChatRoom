#ifndef CHAT_ACCOUNT_HANDLER_H
#define CHAT_ACCOUNT_HANDLER_H

#include <string>
#include <unordered_map>
#include "json.hpp"
#include "protocol.h"

class Connection;
class ConnectionPool;

class AccountHandler
{
public:
    explicit AccountHandler(std::unordered_map<int, Connection *> *userConnections = nullptr);

    std::string handleRegister(nlohmann::json j);
    std::string handleLogin(nlohmann::json j,
                            std::unordered_map<int, Connection *> *currentConnection,
                            Connection *tcpConnection,
                            int &_userId);
    std::string handleResetPassword(nlohmann::json j);

    std::string handleUpdateAvatar(nlohmann::json j,
                                   std::unordered_map<int, Connection *> *currentConnection,
                                   int userId);

    std::string handleCreateGroup(nlohmann::json j, int userId);
    std::string handleJoinGroup(nlohmann::json j, int userId);
    std::string handleGroupManage(nlohmann::json j, int userId);

    std::string retMessage(json_rpc_protocol::cmd_type cmd,
                           json_rpc_protocol::cmd_type status,
                           const char *message);

    std::string retMessageWithData(json_rpc_protocol::cmd_type cmd,
                                   json_rpc_protocol::cmd_type status,
                                   const char *message,
                                   const nlohmann::json &data);

    void notifyFriendOnlineStatus(int userId, int onlineStatus);
    void notifyFriendAvatarChanged(int userId, const std::string &avatarUrl);
    void notifyGroupMemberChanged(int groupId,
                                  const std::string &action,
                                  int targetUserId,
                                  int selfRole,
                                  int selfMuteStatus);

private:
    ConnectionPool *_cpool;
    std::unordered_map<int, Connection *> *_userConnections;
};

#endif