#ifndef CHAT_ACCOUNT_HANDLER_H
#define CHAT_ACCOUNT_HANDLER_H

#include <string>
#include <unordered_map>
#include "json.hpp"
#include "protocol.h"

class Connection;
class ConnectionPool;
class MysqlConnection;

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

    std::string handleSearchUser(nlohmann::json j, int userId);
    std::string handleAddFriend(nlohmann::json j, int userId);
    std::string handleGetFriendRequestList(int userId);
    std::string handleFriendRequest(nlohmann::json j, int userId);

    std::string handleInviteGroupMember(nlohmann::json j, int userId);
    std::string handleGetGroupMembers(nlohmann::json j, int userId);
    std::string handleDeleteFriend(nlohmann::json j, int userId);
    
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
                                  int targetUserId);
                                  
    void notifyFriendDeleted(int toUserId, const nlohmann::json &data);
    void notifyFriendRequest(int toUserId, const nlohmann::json &requestData);
    void notifyFriendRequestResult(int toUserId, const nlohmann::json &resultData);
    void notifyGroupInvite(int toUserId, const nlohmann::json &inviteData);
    void notifyGroupInviteResult(int toUserId, const nlohmann::json &resultData);

private:
    bool isFriend(MysqlConnection *conn, int userId, int friendId);
    bool queryUserByAccount(MysqlConnection *conn,
                            const std::string &account,
                            int &userId,
                            std::string &nickname,
                            std::string &avatar);
    bool queryUserBasic(MysqlConnection *conn,
                        int userId,
                        std::string &account,
                        std::string &nickname,
                        std::string &avatar,
                        int *status = nullptr);
    int queryGroupRole(MysqlConnection *conn, int groupId, int userId, int *muteStatus = nullptr);
    bool queryGroupBasic(MysqlConnection *conn,
                         int groupId,
                         std::string &groupName,
                         std::string &groupAvatar,
                         int &creatorId);
    int countGroupAdmins(MysqlConnection *conn, int groupId);

private:
    ConnectionPool *_cpool;
    std::unordered_map<int, Connection *> *_userConnections;
};

#endif