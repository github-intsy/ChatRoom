#include "ChatAccountHandler.h"
#include "ConnectionPool.h"
#include "Connection.h"
#include "protocol.h"
#include "MysqlConnection.h"
#include "util.h"

#include <mysql/mysql.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <fstream>
#include <filesystem>

namespace
{
    const std::string kAvatarHttpBase = "http://192.168.137.144:8080";

    std::string safeString(const char *s)
    {
        return s ? s : "";
    }

    std::string escapeSql(MysqlConnection *conn, const std::string &input)
    {
        if (!conn || !conn->getRawConnection())
            return input;

        std::string out;
        out.resize(input.size() * 2 + 1);

        unsigned long len = mysql_real_escape_string(
            conn->getRawConnection(),
            &out[0],
            input.c_str(),
            static_cast<unsigned long>(input.size()));

        out.resize(len);
        return out;
    }

    bool isNumericAccount(const std::string &account)
    {
        if (account.size() < 6 || account.size() > 13)
            return false;

        for (char ch : account)
        {
            if (!std::isdigit(static_cast<unsigned char>(ch)))
                return false;
        }
        return true;
    }

    bool isAllowedImageExt(const std::string &ext)
    {
        return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "webp";
    }

    std::string buildAvatarUrl(const std::string &dbPath)
    {
        if (dbPath.empty())
            return "";

        if (dbPath.rfind("http://", 0) == 0 || dbPath.rfind("https://", 0) == 0)
            return dbPath;

        if (dbPath[0] == '/')
            return kAvatarHttpBase + dbPath;

        return kAvatarHttpBase + "/" + dbPath;
    }

    std::string normalizeAvatarForClient(const std::string &dbValue)
    {
        if (dbValue.empty())
            return "";

        if (dbValue.rfind("/static/", 0) == 0 || dbValue.rfind("static/", 0) == 0)
            return buildAvatarUrl(dbValue);

        if (dbValue.rfind("http://", 0) == 0 || dbValue.rfind("https://", 0) == 0)
            return dbValue;

        return dbValue;
    }
}

AccountHandler::AccountHandler(std::unordered_map<int, Connection *> *userConnections)
    : _cpool(ConnectionPool::getConnectionPool()),
      _userConnections(userConnections)
{
}

std::string AccountHandler::retMessage(json_rpc_protocol::cmd_type cmd,
                                       json_rpc_protocol::cmd_type status,
                                       const char *message)
{
    nlohmann::json response;
    response["cmd"] = cmd;
    response["status"] = status;
    response["message"] = message;
    return response.dump();
}

std::string AccountHandler::retMessageWithData(json_rpc_protocol::cmd_type cmd,
                                               json_rpc_protocol::cmd_type status,
                                               const char *message,
                                               const nlohmann::json &data)
{
    nlohmann::json response;
    response["cmd"] = cmd;
    response["status"] = status;
    response["message"] = message;
    response["data"] = data;
    return response.dump();
}

/* ========================= 已有逻辑：注册/登录/头像/建群/加群 ========================= */
/* 你可以保留你原来的这几段；下面直接放完整版本，便于整体替换 */

std::string AccountHandler::handleLogin(nlohmann::json j,
                                        std::unordered_map<int, Connection *> *currentConnection,
                                        Connection *tcpConnection,
                                        int &_userId)
{
    _userConnections = currentConnection;
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_LOGIN_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    std::string account = j.value("account", "");
    std::string password = j.value("password", "");

    if (account.empty() || password.empty())
        return retMessage(cmd_type::CMD_LOGIN_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "参数错误");

    if (!isNumericAccount(account))
        return retMessage(cmd_type::CMD_LOGIN_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "账号必须是6到13位纯数字");
    std::string escAccount = escapeSql(conn.get(), account);

    char sql[8192];
    snprintf(sql, sizeof(sql),
             "SELECT user_id, account, password_hash, nickname, avatar, status "
             "FROM sys_user WHERE account='%s'",
             escAccount.c_str());

    MYSQL_RES *res = conn->query(sql);
    if (!res)
        return retMessage(cmd_type::CMD_LOGIN_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库查询失败");

    if (mysql_num_rows(res) == 0)
    {
        mysql_free_result(res);
        return retMessage(cmd_type::CMD_LOGIN_RES,
                          cmd_type::STATUS_ACCOUNT_NOT_EXISTS,
                          "账号不存在");
    }

    MYSQL_ROW row = mysql_fetch_row(res);

    int userId = row[0] ? atoi(row[0]) : 0;
    std::string dbAccount = safeString(row[1]);
    std::string dbPwd = safeString(row[2]);
    std::string nickname = safeString(row[3]);
    std::string avatar = normalizeAvatarForClient(safeString(row[4]));

    mysql_free_result(res);

    if (dbPwd != password)
        return retMessage(cmd_type::CMD_LOGIN_RES,
                          cmd_type::STATUS_PASSWORD_ERROR,
                          "密码错误");

    (*_userConnections)[userId] = tcpConnection;
    tcpConnection->setUserId(userId);
    _userId = userId;

    long long tmpId = 0;
    snprintf(sql, sizeof(sql),
             "UPDATE sys_user SET status=1 WHERE user_id=%d",
             userId);
    conn->update(sql, tmpId);

    notifyFriendOnlineStatus(userId, 1);

    nlohmann::json data;
    data["user_info"] = {
        {"user_id", userId},
        {"account", dbAccount},
        {"nickname", nickname},
        {"avatar", avatar},
        {"status", 1}};

    nlohmann::json friends = nlohmann::json::array();
    snprintf(sql, sizeof(sql),
             "SELECT u.user_id, u.account, u.nickname, IFNULL(u.avatar,''), u.status "
             "FROM sys_friend_relation fr "
             "JOIN sys_user u ON fr.friend_id = u.user_id "
             "WHERE fr.user_id = %d "
             "ORDER BY u.user_id ASC",
             userId);

    res = conn->query(sql);
    if (res)
    {
        while ((row = mysql_fetch_row(res)))
        {
            nlohmann::json f;
            f["friend_id"] = row[0] ? atoi(row[0]) : 0;
            f["account"] = safeString(row[1]);
            f["nickname"] = safeString(row[2]);
            f["avatar"] = normalizeAvatarForClient(safeString(row[3]));
            f["online_status"] = row[4] ? atoi(row[4]) : 0;
            friends.push_back(f);
        }
        mysql_free_result(res);
    }
    data["friends"] = friends;

    nlohmann::json groups = nlohmann::json::array();
    snprintf(sql, sizeof(sql),
             "SELECT g.group_id, g.group_name, IFNULL(g.avatar,''), gm.role, gm.mute_status "
             "FROM sys_group_member gm "
             "JOIN sys_group g ON gm.group_id = g.group_id "
             "WHERE gm.user_id = %d "
             "ORDER BY g.group_id ASC",
             userId);

    res = conn->query(sql);
    if (res)
    {
        while ((row = mysql_fetch_row(res)))
        {
            nlohmann::json g;
            g["group_id"] = row[0] ? atoi(row[0]) : 0;
            g["group_name"] = safeString(row[1]);
            g["group_avatar"] = normalizeAvatarForClient(safeString(row[2]));
            g["role"] = row[3] ? atoi(row[3]) : 0;
            g["mute_status"] = row[4] ? atoi(row[4]) : 0;
            groups.push_back(g);
        }
        mysql_free_result(res);
    }
    data["groups"] = groups;

    nlohmann::json offlineMessages = nlohmann::json::array();
    snprintf(sql, sizeof(sql),
             "SELECT om.offline_id, om.sender_id, om.receiver_id, om.msg_type, om.content, "
             "om.filename, om.send_time, u.nickname, IFNULL(u.avatar,''), u.account "
             "FROM sys_offline_message om "
             "JOIN sys_user u ON om.sender_id = u.user_id "
             "WHERE om.receiver_id = %d AND om.read_flag = 0 "
             "ORDER BY om.send_time ASC, om.offline_id ASC",
             userId);

    res = conn->query(sql);
    if (res)
    {
        while ((row = mysql_fetch_row(res)))
        {
            nlohmann::json m;
            m["msg_id"] = row[0] ? atoll(row[0]) : 0;
            m["sender_id"] = row[1] ? atoi(row[1]) : 0;
            m["receiver_id"] = row[2] ? atoi(row[2]) : 0;
            m["chat_type"] = "friend";
            m["group_id"] = -1;
            m["msg_type"] = row[3] ? atoi(row[3]) : 1;
            m["content"] = safeString(row[4]);
            m["filename"] = safeString(row[5]);
            m["send_time"] = safeString(row[6]);
            m["sender_name"] = safeString(row[7]);
            m["sender_avatar"] = normalizeAvatarForClient(safeString(row[8]));
            m["target_account"] = safeString(row[9]);
            offlineMessages.push_back(m);
        }
        mysql_free_result(res);
    }
    data["offline_messages"] = offlineMessages;

    snprintf(sql, sizeof(sql),
             "UPDATE sys_offline_message SET read_flag=1 WHERE receiver_id=%d AND read_flag=0",
             userId);
    conn->update(sql, tmpId);

    return retMessageWithData(cmd_type::CMD_LOGIN_RES,
                              cmd_type::STATUS_SUCCESS,
                              "登录成功",
                              data);
}

std::string AccountHandler::handleUpdateAvatar(nlohmann::json j,
                                               std::unordered_map<int, Connection *> *currentConnection,
                                               int userId)
{
    _userConnections = currentConnection;
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_UPDATE_AVATAR_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    if (userId <= 0)
        return retMessage(cmd_type::CMD_UPDATE_AVATAR_RES,
                          cmd_type::STATUS_NOT_LOGIN,
                          "请先登录");

    std::string avatarBase64 = j.value("avatar_base64", "");
    std::string fileExt = j.value("file_ext", "");

    if (avatarBase64.empty())
        return retMessage(cmd_type::CMD_UPDATE_AVATAR_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "头像不能为空");

    for (auto &ch : fileExt)
        ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));

    if (fileExt == "jpeg")
        fileExt = "jpg";

    if (!isAllowedImageExt(fileExt))
        return retMessage(cmd_type::CMD_UPDATE_AVATAR_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "头像格式不支持");

    std::string imageData = base64Decode(avatarBase64);
    if (imageData.empty())
        return retMessage(cmd_type::CMD_UPDATE_AVATAR_RES,
                          cmd_type::STATUS_FAILED,
                          "头像解码失败");

    try
    {
        std::filesystem::create_directories("./static/avatar");
    }
    catch (...)
    {
        return retMessage(cmd_type::CMD_UPDATE_AVATAR_RES,
                          cmd_type::STATUS_FAILED,
                          "头像目录创建失败");
    }

    std::string fileName = "user_" + std::to_string(userId) + "." + fileExt;
    std::string localPath = "./static/avatar/" + fileName;

    std::ofstream ofs(localPath, std::ios::binary);
    if (!ofs.is_open())
        return retMessage(cmd_type::CMD_UPDATE_AVATAR_RES,
                          cmd_type::STATUS_FAILED,
                          "头像文件保存失败");

    ofs.write(imageData.data(), static_cast<std::streamsize>(imageData.size()));
    ofs.close();

    if (!std::filesystem::exists(localPath))
        return retMessage(cmd_type::CMD_UPDATE_AVATAR_RES,
                          cmd_type::STATUS_FAILED,
                          "头像写入失败");

    std::string avatarPathInDb = "/static/avatar/" + fileName;
    std::string escAvatar = escapeSql(conn.get(), avatarPathInDb);

    char sql[1024];
    long long id = 0;

    snprintf(sql, sizeof(sql),
             "UPDATE sys_user SET avatar='%s' WHERE user_id=%d",
             escAvatar.c_str(), userId);

    if (!conn->update(sql, id))
        return retMessage(cmd_type::CMD_UPDATE_AVATAR_RES,
                          cmd_type::STATUS_FAILED,
                          "更新失败");

    notifyFriendAvatarChanged(userId, buildAvatarUrl(avatarPathInDb));

    nlohmann::json data;
    data["avatar"] = buildAvatarUrl(avatarPathInDb);

    return retMessageWithData(cmd_type::CMD_UPDATE_AVATAR_RES,
                              cmd_type::STATUS_SUCCESS,
                              "更新成功",
                              data);
}

std::string AccountHandler::handleCreateGroup(nlohmann::json j, int userId)
{
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_CREATE_GROUP_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    if (userId <= 0)
        return retMessage(cmd_type::CMD_CREATE_GROUP_RES,
                          cmd_type::STATUS_NOT_LOGIN,
                          "请先登录");

    std::string groupName = j.value("group_name", "");
    std::string groupAvatar = j.value("group_avatar", "");
    if (groupName.empty())
        return retMessage(cmd_type::CMD_CREATE_GROUP_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "群名不能为空");

    std::string escGroupName = escapeSql(conn.get(), groupName);
    std::string escGroupAvatar = escapeSql(conn.get(), groupAvatar);

    char sql[2048];
    long long id = 0;

    snprintf(sql, sizeof(sql),
             "INSERT INTO sys_group(group_name,creator_id,create_time,notice,avatar) "
             "VALUES('%s',%d,NOW(),'','%s')",
             escGroupName.c_str(), userId, escGroupAvatar.c_str());

    if (!conn->update(sql, id))
        return retMessage(cmd_type::CMD_CREATE_GROUP_RES,
                          cmd_type::STATUS_FAILED,
                          "创建失败");

    int groupId = static_cast<int>(id);

    snprintf(sql, sizeof(sql),
             "INSERT INTO sys_group_member(group_id,user_id,role,join_time,mute_status) "
             "VALUES(%d,%d,2,NOW(),0)",
             groupId, userId);

    conn->update(sql, id);

    nlohmann::json data;
    data["group_id"] = groupId;
    data["group_name"] = groupName;
    data["group_avatar"] = normalizeAvatarForClient(groupAvatar);

    return retMessageWithData(cmd_type::CMD_CREATE_GROUP_RES,
                              cmd_type::STATUS_SUCCESS,
                              "创建成功",
                              data);
}

std::string AccountHandler::handleJoinGroup(nlohmann::json j, int userId)
{
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_JOIN_GROUP_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    int groupId = j.value("group_id", 0);
    if (groupId <= 0)
        return retMessage(cmd_type::CMD_JOIN_GROUP_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "群ID无效");

    std::string groupName, groupAvatar;
    int creatorId = 0;
    if (!queryGroupBasic(conn.get(), groupId, groupName, groupAvatar, creatorId))
        return retMessage(cmd_type::CMD_JOIN_GROUP_RES,
                          cmd_type::STATUS_GROUP_NOT_EXISTS,
                          "群不存在");

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT member_id FROM sys_group_member WHERE group_id=%d AND user_id=%d",
             groupId, userId);

    MYSQL_RES *res = conn->query(sql);
    if (res && mysql_num_rows(res) > 0)
    {
        mysql_free_result(res);
        return retMessage(cmd_type::CMD_JOIN_GROUP_RES,
                          cmd_type::STATUS_ALREADY_IN_GROUP,
                          "你已在该群中");
    }
    if (res)
        mysql_free_result(res);

    long long id = 0;
    snprintf(sql, sizeof(sql),
             "INSERT INTO sys_group_member(group_id,user_id,role,join_time,mute_status) "
             "VALUES(%d,%d,0,NOW(),0)",
             groupId, userId);

    if (!conn->update(sql, id))
        return retMessage(cmd_type::CMD_JOIN_GROUP_RES,
                          cmd_type::STATUS_FAILED,
                          "加入失败");

    nlohmann::json data;
    data["group_id"] = groupId;
    data["group_name"] = groupName;
    data["group_avatar"] = normalizeAvatarForClient(groupAvatar);

    notifyGroupMemberChanged(groupId, "join", userId);

    return retMessageWithData(cmd_type::CMD_JOIN_GROUP_RES,
                              cmd_type::STATUS_SUCCESS,
                              "加入成功",
                              data);
}

/* ========================= 工具查询 ========================= */

bool AccountHandler::queryUserByAccount(MysqlConnection *conn,
                                        const std::string &account,
                                        int &userId,
                                        std::string &nickname,
                                        std::string &avatar)
{
    if (!conn || account.empty())
        return false;

    std::string esc = escapeSql(conn, account);
    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT user_id,nickname,IFNULL(avatar,'') FROM sys_user WHERE account='%s'",
             esc.c_str());

    MYSQL_RES *res = conn->query(sql);
    if (!res)
        return false;

    bool ok = false;
    if (mysql_num_rows(res) > 0)
    {
        MYSQL_ROW row = mysql_fetch_row(res);
        userId = row[0] ? atoi(row[0]) : 0;
        nickname = safeString(row[1]);
        avatar = normalizeAvatarForClient(safeString(row[2]));
        ok = true;
    }
    mysql_free_result(res);
    return ok;
}

bool AccountHandler::queryUserBasic(MysqlConnection *conn,
                                    int userId,
                                    std::string &account,
                                    std::string &nickname,
                                    std::string &avatar,
                                    int *status)
{
    if (!conn || userId <= 0)
        return false;

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT account,nickname,IFNULL(avatar,''),status FROM sys_user WHERE user_id=%d",
             userId);

    MYSQL_RES *res = conn->query(sql);
    if (!res)
        return false;

    bool ok = false;
    if (mysql_num_rows(res) > 0)
    {
        MYSQL_ROW row = mysql_fetch_row(res);
        account = safeString(row[0]);
        nickname = safeString(row[1]);
        avatar = normalizeAvatarForClient(safeString(row[2]));
        if (status)
            *status = row[3] ? atoi(row[3]) : 0;
        ok = true;
    }
    mysql_free_result(res);
    return ok;
}

bool AccountHandler::isFriend(MysqlConnection *conn, int userId, int friendId)
{
    if (!conn || userId <= 0 || friendId <= 0)
        return false;

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT relation_id FROM sys_friend_relation WHERE user_id=%d AND friend_id=%d",
             userId, friendId);

    MYSQL_RES *res = conn->query(sql);
    bool ok = (res && mysql_num_rows(res) > 0);
    if (res)
        mysql_free_result(res);
    return ok;
}

int AccountHandler::queryGroupRole(MysqlConnection *conn, int groupId, int userId, int *muteStatus)
{
    if (!conn || groupId <= 0 || userId <= 0)
        return -1;

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT role,mute_status FROM sys_group_member WHERE group_id=%d AND user_id=%d",
             groupId, userId);

    MYSQL_RES *res = conn->query(sql);
    if (!res)
        return -1;

    int role = -1;
    if (mysql_num_rows(res) > 0)
    {
        MYSQL_ROW row = mysql_fetch_row(res);
        role = row[0] ? atoi(row[0]) : 0;
        if (muteStatus)
            *muteStatus = row[1] ? atoi(row[1]) : 0;
    }
    mysql_free_result(res);
    return role;
}

bool AccountHandler::queryGroupBasic(MysqlConnection *conn,
                                     int groupId,
                                     std::string &groupName,
                                     std::string &groupAvatar,
                                     int &creatorId)
{
    if (!conn || groupId <= 0)
        return false;

    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT group_name,IFNULL(avatar,''),creator_id FROM sys_group WHERE group_id=%d",
             groupId);

    MYSQL_RES *res = conn->query(sql);
    if (!res)
        return false;

    bool ok = false;
    if (mysql_num_rows(res) > 0)
    {
        MYSQL_ROW row = mysql_fetch_row(res);
        groupName = safeString(row[0]);
        groupAvatar = safeString(row[1]);
        creatorId = row[2] ? atoi(row[2]) : 0;
        ok = true;
    }
    mysql_free_result(res);
    return ok;
}

int AccountHandler::countGroupAdmins(MysqlConnection *conn, int groupId)
{
    if (!conn || groupId <= 0)
        return 0;

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT COUNT(*) FROM sys_group_member WHERE group_id=%d AND role=1",
             groupId);

    MYSQL_RES *res = conn->query(sql);
    int cnt = 0;
    if (res && mysql_num_rows(res) > 0)
    {
        MYSQL_ROW row = mysql_fetch_row(res);
        cnt = row[0] ? atoi(row[0]) : 0;
    }
    if (res)
        mysql_free_result(res);
    return cnt;
}

/* ========================= 搜索用户 ========================= */

std::string AccountHandler::handleSearchUser(nlohmann::json j, int userId)
{
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_SEARCH_USER_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    if (userId <= 0)
        return retMessage(cmd_type::CMD_SEARCH_USER_RES,
                          cmd_type::STATUS_NOT_LOGIN,
                          "请先登录");

    std::string keyword = j.value("account", "");
    if (keyword.empty())
        return retMessage(cmd_type::CMD_SEARCH_USER_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "请输入账号");

    std::string esc = escapeSql(conn.get(), keyword);

    char sql[2048];
    snprintf(sql, sizeof(sql),
             "SELECT user_id,account,nickname,IFNULL(avatar,''),status "
             "FROM sys_user WHERE account LIKE '%%%s%%' ORDER BY user_id ASC LIMIT 20",
             esc.c_str());

    MYSQL_RES *res = conn->query(sql);
    if (!res)
        return retMessage(cmd_type::CMD_SEARCH_USER_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "查询失败");

    nlohmann::json arr = nlohmann::json::array();
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        int targetId = row[0] ? atoi(row[0]) : 0;
        if (targetId == userId)
            continue;

        nlohmann::json item;
        item["user_id"] = targetId;
        item["account"] = safeString(row[1]);
        item["nickname"] = safeString(row[2]);
        item["avatar"] = normalizeAvatarForClient(safeString(row[3]));
        item["status"] = row[4] ? atoi(row[4]) : 0;
        item["is_friend"] = isFriend(conn.get(), userId, targetId);
        arr.push_back(item);
    }
    mysql_free_result(res);

    nlohmann::json data;
    data["users"] = arr;

    return retMessageWithData(cmd_type::CMD_SEARCH_USER_RES,
                              cmd_type::STATUS_SUCCESS,
                              "查询成功",
                              data);
}

/* ========================= 添加好友申请 ========================= */

std::string AccountHandler::handleAddFriend(nlohmann::json j, int userId)
{
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_ADD_FRIEND_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    if (userId <= 0)
        return retMessage(cmd_type::CMD_ADD_FRIEND_RES,
                          cmd_type::STATUS_NOT_LOGIN,
                          "请先登录");

    std::string targetAccount = j.value("target_account", "");
    std::string requestMessage = j.value("message", "");

    if (targetAccount.empty())
        return retMessage(cmd_type::CMD_ADD_FRIEND_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "目标账号不能为空");

    int targetUserId = 0;
    std::string targetNickname, targetAvatar;
    if (!queryUserByAccount(conn.get(), targetAccount, targetUserId, targetNickname, targetAvatar))
        return retMessage(cmd_type::CMD_ADD_FRIEND_RES,
                          cmd_type::STATUS_ACCOUNT_NOT_EXISTS,
                          "目标账号不存在");

    if (targetUserId == userId)
        return retMessage(cmd_type::CMD_ADD_FRIEND_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "不能添加自己");

    if (isFriend(conn.get(), userId, targetUserId))
        return retMessage(cmd_type::CMD_ADD_FRIEND_RES,
                          cmd_type::STATUS_ALREADY_FRIEND,
                          "已经是好友了");

    char sql[2048];
    snprintf(sql, sizeof(sql),
             "SELECT request_id FROM sys_friend_request "
             "WHERE from_user_id=%d AND to_user_id=%d AND status=0",
             userId, targetUserId);

    MYSQL_RES *res = conn->query(sql);
    if (res && mysql_num_rows(res) > 0)
    {
        mysql_free_result(res);
        return retMessage(cmd_type::CMD_ADD_FRIEND_RES,
                          cmd_type::STATUS_FRIEND_REQUEST_EXISTS,
                          "好友申请已发送，请勿重复提交");
    }
    if (res)
        mysql_free_result(res);

    std::string escMsg = escapeSql(conn.get(), requestMessage);
    long long insertId = 0;
    snprintf(sql, sizeof(sql),
             "INSERT INTO sys_friend_request(from_user_id,to_user_id,message,status,create_time) "
             "VALUES(%d,%d,'%s',0,NOW())",
             userId, targetUserId, escMsg.c_str());

    if (!conn->update(sql, insertId))
        return retMessage(cmd_type::CMD_ADD_FRIEND_RES,
                          cmd_type::STATUS_FAILED,
                          "好友申请发送失败");

    std::string fromAccount, fromNickname, fromAvatar;
    int fromStatus = 0;
    queryUserBasic(conn.get(), userId, fromAccount, fromNickname, fromAvatar, &fromStatus);

    nlohmann::json notifyData;
    notifyData["request_id"] = insertId;
    notifyData["from_user_id"] = userId;
    notifyData["from_account"] = fromAccount;
    notifyData["from_nickname"] = fromNickname;
    notifyData["from_avatar"] = fromAvatar;
    notifyData["message"] = requestMessage;
    notifyData["create_time"] = "";

    notifyFriendRequest(targetUserId, notifyData);

    nlohmann::json data;
    data["request_id"] = insertId;
    data["target_user_id"] = targetUserId;

    return retMessageWithData(cmd_type::CMD_ADD_FRIEND_RES,
                              cmd_type::STATUS_SUCCESS,
                              "好友申请已发送",
                              data);
}

/* ========================= 获取好友申请列表 ========================= */

std::string AccountHandler::handleGetFriendRequestList(int userId)
{
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_GET_FRIEND_REQUEST_LIST_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    if (userId <= 0)
        return retMessage(cmd_type::CMD_GET_FRIEND_REQUEST_LIST_RES,
                          cmd_type::STATUS_NOT_LOGIN,
                          "请先登录");

    char sql[4096];
    snprintf(sql, sizeof(sql),
             "SELECT r.request_id,r.from_user_id,u.account,u.nickname,IFNULL(u.avatar,''),r.message,r.status,r.create_time "
             "FROM sys_friend_request r "
             "JOIN sys_user u ON r.from_user_id=u.user_id "
             "WHERE r.to_user_id=%d ORDER BY r.request_id DESC",
             userId);

    MYSQL_RES *res = conn->query(sql);
    if (!res)
        return retMessage(cmd_type::CMD_GET_FRIEND_REQUEST_LIST_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "查询失败");

    nlohmann::json arr = nlohmann::json::array();
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        nlohmann::json item;
        item["request_id"] = row[0] ? atoll(row[0]) : 0;
        item["from_user_id"] = row[1] ? atoi(row[1]) : 0;
        item["from_account"] = safeString(row[2]);
        item["from_nickname"] = safeString(row[3]);
        item["from_avatar"] = normalizeAvatarForClient(safeString(row[4]));
        item["message"] = safeString(row[5]);
        item["status"] = row[6] ? atoi(row[6]) : 0;
        item["create_time"] = safeString(row[7]);
        arr.push_back(item);
    }
    mysql_free_result(res);

    nlohmann::json data;
    data["requests"] = arr;

    return retMessageWithData(cmd_type::CMD_GET_FRIEND_REQUEST_LIST_RES,
                              cmd_type::STATUS_SUCCESS,
                              "获取成功",
                              data);
}

/* ========================= 处理好友申请 ========================= */

std::string AccountHandler::handleFriendRequest(nlohmann::json j, int userId)
{
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_HANDLE_FRIEND_REQUEST_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    if (userId <= 0)
        return retMessage(cmd_type::CMD_HANDLE_FRIEND_REQUEST_RES,
                          cmd_type::STATUS_NOT_LOGIN,
                          "请先登录");

    long long requestId = j.value("request_id", 0LL);
    int action = j.value("action", 0); // 1同意 2拒绝

    if (requestId <= 0 || (action != 1 && action != 2))
        return retMessage(cmd_type::CMD_HANDLE_FRIEND_REQUEST_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "参数错误");

    char sql[4096];
    snprintf(sql, sizeof(sql),
             "SELECT from_user_id,to_user_id,status FROM sys_friend_request WHERE request_id=%lld",
             requestId);

    MYSQL_RES *res = conn->query(sql);
    if (!res || mysql_num_rows(res) == 0)
    {
        if (res)
            mysql_free_result(res);
        return retMessage(cmd_type::CMD_HANDLE_FRIEND_REQUEST_RES,
                          cmd_type::STATUS_FRIEND_REQUEST_NOT_EXISTS,
                          "好友申请不存在");
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    int fromUserId = row[0] ? atoi(row[0]) : 0;
    int toUserId = row[1] ? atoi(row[1]) : 0;
    int status = row[2] ? atoi(row[2]) : 0;
    mysql_free_result(res);

    if (toUserId != userId || status != 0)
        return retMessage(cmd_type::CMD_HANDLE_FRIEND_REQUEST_RES,
                          cmd_type::STATUS_NO_PERMISSION,
                          "该申请不可处理");

    long long tmpId = 0;
    snprintf(sql, sizeof(sql),
             "UPDATE sys_friend_request SET status=%d WHERE request_id=%lld",
             action, requestId);

    if (!conn->update(sql, tmpId))
        return retMessage(cmd_type::CMD_HANDLE_FRIEND_REQUEST_RES,
                          cmd_type::STATUS_FAILED,
                          "处理失败");

    if (action == 1)
    {
        if (!isFriend(conn.get(), fromUserId, toUserId))
        {
            snprintf(sql, sizeof(sql),
                     "INSERT INTO sys_friend_relation(user_id,friend_id,create_time) "
                     "VALUES(%d,%d,NOW())",
                     fromUserId, toUserId);
            conn->update(sql, tmpId);
        }

        if (!isFriend(conn.get(), toUserId, fromUserId))
        {
            snprintf(sql, sizeof(sql),
                     "INSERT INTO sys_friend_relation(user_id,friend_id,create_time) "
                     "VALUES(%d,%d,NOW())",
                     toUserId, fromUserId);
            conn->update(sql, tmpId);
        }
    }

    std::string selfAccount, selfNickname, selfAvatar;
    int selfStatus = 0;
    queryUserBasic(conn.get(), userId, selfAccount, selfNickname, selfAvatar, &selfStatus);

    std::string peerAccount, peerNickname, peerAvatar;
    int peerStatus = 0;
    queryUserBasic(conn.get(), fromUserId, peerAccount, peerNickname, peerAvatar, &peerStatus);

    nlohmann::json notifyData;
    notifyData["request_id"] = requestId;
    notifyData["result"] = action;
    notifyData["user_id"] = userId;
    notifyData["account"] = selfAccount;
    notifyData["nickname"] = selfNickname;
    notifyData["avatar"] = selfAvatar;

    notifyFriendRequestResult(fromUserId, notifyData);

    nlohmann::json data;
    data["request_id"] = requestId;
    data["result"] = action;

    if (action == 1)
    {
        data["friend"] = {
            {"friend_id", fromUserId},
            {"account", peerAccount},
            {"nickname", peerNickname},
            {"avatar", peerAvatar},
            {"online_status", peerStatus}};
    }

    return retMessageWithData(cmd_type::CMD_HANDLE_FRIEND_REQUEST_RES,
                              cmd_type::STATUS_SUCCESS,
                              action == 1 ? "已同意好友申请" : "已拒绝好友申请",
                              data);
}

/* ========================= 邀请好友入群 / 同意拒绝邀请 ========================= */

std::string AccountHandler::handleInviteGroupMember(nlohmann::json j, int userId)
{
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_INVITE_GROUP_MEMBER_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    if (userId <= 0)
        return retMessage(cmd_type::CMD_INVITE_GROUP_MEMBER_RES,
                          cmd_type::STATUS_NOT_LOGIN,
                          "请先登录");

    int groupId = j.value("group_id", 0);
    std::string mode = j.value("mode", "invite");   // invite / handle
    std::string action = j.value("action", "");     // accept / reject
    long long inviteId = j.value("invite_id", 0LL); // handle时用
    std::string targetAccount = j.value("target_account", "");
    std::string message = j.value("message", "");

    if (mode == "invite")
    {
        int selfRole = queryGroupRole(conn.get(), groupId, userId, nullptr);
        if (selfRole < 1)
            return retMessage(cmd_type::CMD_INVITE_GROUP_MEMBER_RES,
                              cmd_type::STATUS_NO_PERMISSION,
                              "只有群主或管理员可以邀请成员");

        int targetUserId = 0;
        std::string targetNickname, targetAvatar;
        if (!queryUserByAccount(conn.get(), targetAccount, targetUserId, targetNickname, targetAvatar))
            return retMessage(cmd_type::CMD_INVITE_GROUP_MEMBER_RES,
                              cmd_type::STATUS_ACCOUNT_NOT_EXISTS,
                              "目标账号不存在");

        if (queryGroupRole(conn.get(), groupId, targetUserId, nullptr) >= 0)
        {
            char sql[4096];

            snprintf(sql, sizeof(sql),
                     "SELECT invite_id FROM sys_group_invite "
                     "WHERE group_id=%d AND invitee_id=%d AND status=0",
                     groupId, targetUserId);

            MYSQL_RES *res = conn->query(sql);
            if (res && mysql_num_rows(res) > 0)
            {
                mysql_free_result(res);
                return retMessage(cmd_type::CMD_INVITE_GROUP_MEMBER_RES,
                                  cmd_type::STATUS_FAILED,
                                  "该用户已有待处理入群邀请");
            }
            if (res)
                mysql_free_result(res);
            return retMessage(cmd_type::CMD_INVITE_GROUP_MEMBER_RES,
                              cmd_type::STATUS_TARGET_ALREADY_IN_GROUP,
                              "该用户已在群中");
        }

        char sql[4096];
        snprintf(sql, sizeof(sql),
                 "CREATE TABLE IF NOT EXISTS sys_group_invite ("
                 "invite_id BIGINT NOT NULL AUTO_INCREMENT PRIMARY KEY,"
                 "group_id BIGINT NOT NULL,"
                 "inviter_id BIGINT NOT NULL,"
                 "invitee_id BIGINT NOT NULL,"
                 "message VARCHAR(255) NOT NULL DEFAULT '',"
                 "status TINYINT NOT NULL DEFAULT 0,"
                 "create_time DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP)");

        long long tmp = 0;
        conn->update(sql, tmp);

        std::string escMsg = escapeSql(conn.get(), message);
        snprintf(sql, sizeof(sql),
                 "INSERT INTO sys_group_invite(group_id,inviter_id,invitee_id,message,status,create_time) "
                 "VALUES(%d,%d,%d,'%s',0,NOW())",
                 groupId, userId, targetUserId, escMsg.c_str());

        long long newInviteId = 0;
        if (!conn->update(sql, newInviteId))
            return retMessage(cmd_type::CMD_INVITE_GROUP_MEMBER_RES,
                              cmd_type::STATUS_FAILED,
                              "邀请失败");

        std::string groupName, groupAvatar;
        int creatorId = 0;
        queryGroupBasic(conn.get(), groupId, groupName, groupAvatar, creatorId);

        std::string inviterAccount, inviterNickname, inviterAvatar;
        queryUserBasic(conn.get(), userId, inviterAccount, inviterNickname, inviterAvatar, nullptr);

        nlohmann::json notifyData;
        notifyData["invite_id"] = newInviteId;
        notifyData["group_id"] = groupId;
        notifyData["group_name"] = groupName;
        notifyData["group_avatar"] = normalizeAvatarForClient(groupAvatar);
        notifyData["inviter_id"] = userId;
        notifyData["inviter_account"] = inviterAccount;
        notifyData["inviter_nickname"] = inviterNickname;
        notifyData["inviter_avatar"] = inviterAvatar;
        notifyData["message"] = message;

        notifyGroupInvite(targetUserId, notifyData);

        nlohmann::json data;
        data["invite_id"] = newInviteId;
        data["group_id"] = groupId;

        return retMessageWithData(cmd_type::CMD_INVITE_GROUP_MEMBER_RES,
                                  cmd_type::STATUS_SUCCESS,
                                  "邀请已发送",
                                  data);
    }

    if (mode == "handle")
    {
        if (inviteId <= 0 || (action != "accept" && action != "reject"))
            return retMessage(cmd_type::CMD_INVITE_GROUP_MEMBER_RES,
                              cmd_type::STATUS_INVALID_PARAMS,
                              "参数错误");

        char sql[4096];
        snprintf(sql, sizeof(sql),
                 "SELECT group_id,inviter_id,invitee_id,status FROM sys_group_invite WHERE invite_id=%lld",
                 inviteId);

        MYSQL_RES *res = conn->query(sql);
        if (!res || mysql_num_rows(res) == 0)
        {
            if (res)
                mysql_free_result(res);
            return retMessage(cmd_type::CMD_INVITE_GROUP_MEMBER_RES,
                              cmd_type::STATUS_FAILED,
                              "邀请不存在");
        }

        MYSQL_ROW row = mysql_fetch_row(res);
        int inviteGroupId = row[0] ? atoi(row[0]) : 0;
        int inviterId = row[1] ? atoi(row[1]) : 0;
        int inviteeId = row[2] ? atoi(row[2]) : 0;
        int status = row[3] ? atoi(row[3]) : 0;
        mysql_free_result(res);

        if (inviteeId != userId || status != 0)
            return retMessage(cmd_type::CMD_INVITE_GROUP_MEMBER_RES,
                              cmd_type::STATUS_NO_PERMISSION,
                              "该邀请不可处理");

        long long tmp = 0;
        snprintf(sql, sizeof(sql),
                 "UPDATE sys_group_invite SET status=%d WHERE invite_id=%lld",
                 action == "accept" ? 1 : 2, inviteId);
        conn->update(sql, tmp);

        if (action == "accept" && queryGroupRole(conn.get(), inviteGroupId, userId, nullptr) < 0)
        {
            snprintf(sql, sizeof(sql),
                     "INSERT INTO sys_group_member(group_id,user_id,role,join_time,mute_status) "
                     "VALUES(%d,%d,0,NOW(),0)",
                     inviteGroupId, userId);
            conn->update(sql, tmp);
            notifyGroupMemberChanged(inviteGroupId, "join", userId);
        }

        std::string account, nickname, avatar;
        queryUserBasic(conn.get(), userId, account, nickname, avatar, nullptr);

        std::string groupName, groupAvatar;
        int creatorId = 0;
        queryGroupBasic(conn.get(), inviteGroupId, groupName, groupAvatar, creatorId);

        nlohmann::json resultData;
        resultData["invite_id"] = inviteId;
        resultData["group_id"] = inviteGroupId;
        resultData["group_name"] = groupName;
        resultData["group_avatar"] = normalizeAvatarForClient(groupAvatar);
        resultData["result"] = action;
        resultData["user_id"] = userId;
        resultData["account"] = account;
        resultData["nickname"] = nickname;
        resultData["avatar"] = avatar;

        notifyGroupInviteResult(inviterId, resultData);

        return retMessageWithData(cmd_type::CMD_INVITE_GROUP_MEMBER_RES,
                                  cmd_type::STATUS_SUCCESS,
                                  action == "accept" ? "已同意入群邀请" : "已拒绝入群邀请",
                                  resultData);
    }

    return retMessage(cmd_type::CMD_INVITE_GROUP_MEMBER_RES,
                      cmd_type::STATUS_INVALID_PARAMS,
                      "未知操作");
}

/* ========================= 获取群成员 ========================= */

std::string AccountHandler::handleGetGroupMembers(nlohmann::json j, int userId)
{
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_GET_GROUP_MEMBERS_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    int groupId = j.value("group_id", 0);
    if (groupId <= 0)
        return retMessage(cmd_type::CMD_GET_GROUP_MEMBERS_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "群ID无效");

    int selfRole = queryGroupRole(conn.get(), groupId, userId, nullptr);
    if (selfRole < 0)
        return retMessage(cmd_type::CMD_GET_GROUP_MEMBERS_RES,
                          cmd_type::STATUS_NO_PERMISSION,
                          "你不在该群中");

    char sql[4096];
    snprintf(sql, sizeof(sql),
             "SELECT gm.user_id,u.account,u.nickname,IFNULL(u.avatar,''),u.status,gm.role,gm.mute_status "
             "FROM sys_group_member gm "
             "JOIN sys_user u ON gm.user_id=u.user_id "
             "WHERE gm.group_id=%d ORDER BY gm.role DESC, gm.user_id ASC",
             groupId);

    MYSQL_RES *res = conn->query(sql);
    if (!res)
        return retMessage(cmd_type::CMD_GET_GROUP_MEMBERS_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "查询失败");

    nlohmann::json arr = nlohmann::json::array();
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        nlohmann::json item;
        item["user_id"] = row[0] ? atoi(row[0]) : 0;
        item["account"] = safeString(row[1]);
        item["nickname"] = safeString(row[2]);
        item["avatar"] = normalizeAvatarForClient(safeString(row[3]));
        item["online_status"] = row[4] ? atoi(row[4]) : 0;
        item["role"] = row[5] ? atoi(row[5]) : 0;
        item["mute_status"] = row[6] ? atoi(row[6]) : 0;
        arr.push_back(item);
    }
    mysql_free_result(res);

    nlohmann::json data;
    data["group_id"] = groupId;
    data["self_role"] = selfRole;
    data["members"] = arr;

    return retMessageWithData(cmd_type::CMD_GET_GROUP_MEMBERS_RES,
                              cmd_type::STATUS_SUCCESS,
                              "获取成功",
                              data);
}

/* ========================= 群管理 ========================= */

std::string AccountHandler::handleGroupManage(nlohmann::json j, int userId)
{
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_GROUP_MANAGE_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    int groupId = j.value("group_id", 0);
    std::string action = j.value("action", "");
    std::string targetAccount = j.value("target_account", "");

    if (groupId <= 0 || action.empty())
        return retMessage(cmd_type::CMD_GROUP_MANAGE_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "参数错误");

    int selfMute = 0;
    int selfRole = queryGroupRole(conn.get(), groupId, userId, &selfMute);
    if (selfRole < 0)
        return retMessage(cmd_type::CMD_GROUP_MANAGE_RES,
                          cmd_type::STATUS_NO_PERMISSION,
                          "你不在该群中");

    std::string groupName, groupAvatar;
    int creatorId = 0;
    if (!queryGroupBasic(conn.get(), groupId, groupName, groupAvatar, creatorId))
        return retMessage(cmd_type::CMD_GROUP_MANAGE_RES,
                          cmd_type::STATUS_GROUP_NOT_EXISTS,
                          "群不存在");

    char sql[4096];
    long long tmpId = 0;

    if (action == "dismiss")
    {
        if (selfRole != 2)
            return retMessage(cmd_type::CMD_GROUP_MANAGE_RES,
                              cmd_type::STATUS_NO_PERMISSION,
                              "只有群主可以解散群");

        snprintf(sql, sizeof(sql), "DELETE FROM sys_group WHERE group_id=%d", groupId);
        if (!conn->update(sql, tmpId))
            return retMessage(cmd_type::CMD_GROUP_MANAGE_RES,
                              cmd_type::STATUS_FAILED,
                              "解散失败");

        notifyGroupMemberChanged(groupId, "dismiss", userId);

        nlohmann::json data;
        data["group_id"] = groupId;
        data["self_role"] = 2;
        data["self_mute_status"] = 0;
        return retMessageWithData(cmd_type::CMD_GROUP_MANAGE_RES,
                                  cmd_type::STATUS_SUCCESS,
                                  "群已解散",
                                  data);
    }

    int targetUserId = 0;
    std::string targetNickname, targetAvatar;
    if (targetAccount.empty() || !queryUserByAccount(conn.get(), targetAccount, targetUserId, targetNickname, targetAvatar))
        return retMessage(cmd_type::CMD_GROUP_MANAGE_RES,
                          cmd_type::STATUS_ACCOUNT_NOT_EXISTS,
                          "目标账号不存在");

    int targetMute = 0;
    int targetRole = queryGroupRole(conn.get(), groupId, targetUserId, &targetMute);
    if (targetRole < 0)
        return retMessage(cmd_type::CMD_GROUP_MANAGE_RES,
                          cmd_type::STATUS_GROUP_MEMBER_NOT_EXISTS,
                          "目标用户不在群中");

    if (targetUserId == userId)
        return retMessage(cmd_type::CMD_GROUP_MANAGE_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "不能操作自己");

    if (selfRole <= targetRole)
        return retMessage(cmd_type::CMD_GROUP_MANAGE_RES,
                          cmd_type::STATUS_NO_PERMISSION,
                          "只能操作权限低于自己的人");

    if (action == "set_admin")
    {
        if (selfRole != 2)
            return retMessage(cmd_type::CMD_GROUP_MANAGE_RES,
                              cmd_type::STATUS_NO_PERMISSION,
                              "只有群主可以设置管理员");

        if (targetRole != 0)
            return retMessage(cmd_type::CMD_GROUP_MANAGE_RES,
                              cmd_type::STATUS_INVALID_PARAMS,
                              "只能将普通成员设为管理员");

        if (countGroupAdmins(conn.get(), groupId) >= 3)
            return retMessage(cmd_type::CMD_GROUP_MANAGE_RES,
                              cmd_type::STATUS_GROUP_ADMIN_LIMIT,
                              "管理员最多3人");

        snprintf(sql, sizeof(sql),
                 "UPDATE sys_group_member SET role=1 WHERE group_id=%d AND user_id=%d",
                 groupId, targetUserId);
        conn->update(sql, tmpId);

        notifyGroupMemberChanged(groupId, "set_admin", targetUserId);
    }
    else if (action == "unset_admin")
    {
        if (selfRole != 2)
            return retMessage(cmd_type::CMD_GROUP_MANAGE_RES,
                              cmd_type::STATUS_NO_PERMISSION,
                              "只有群主可以取消管理员");

        if (targetRole != 1)
            return retMessage(cmd_type::CMD_GROUP_MANAGE_RES,
                              cmd_type::STATUS_INVALID_PARAMS,
                              "目标不是管理员");

        snprintf(sql, sizeof(sql),
                 "UPDATE sys_group_member SET role=0 WHERE group_id=%d AND user_id=%d",
                 groupId, targetUserId);
        conn->update(sql, tmpId);

        notifyGroupMemberChanged(groupId, "unset_admin", targetUserId);
    }
    else if (action == "mute")
    {
        snprintf(sql, sizeof(sql),
                 "UPDATE sys_group_member SET mute_status=1 WHERE group_id=%d AND user_id=%d",
                 groupId, targetUserId);
        conn->update(sql, tmpId);

        notifyGroupMemberChanged(groupId, "mute", targetUserId);
    }
    else if (action == "unmute")
    {
        snprintf(sql, sizeof(sql),
                 "UPDATE sys_group_member SET mute_status=0 WHERE group_id=%d AND user_id=%d",
                 groupId, targetUserId);
        conn->update(sql, tmpId);

        notifyGroupMemberChanged(groupId, "unmute", targetUserId);
    }
    else if (action == "kick")
    {
        snprintf(sql, sizeof(sql),
                 "DELETE FROM sys_group_member WHERE group_id=%d AND user_id=%d",
                 groupId, targetUserId);
        conn->update(sql, tmpId);

        notifyGroupMemberChanged(groupId, "kicked", targetUserId);
    }
    else
    {
        return retMessage(cmd_type::CMD_GROUP_MANAGE_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "未知操作");
    }

    nlohmann::json data;
    data["group_id"] = groupId;
    data["self_role"] = selfRole;
    data["self_mute_status"] = selfMute;

    return retMessageWithData(cmd_type::CMD_GROUP_MANAGE_RES,
                              cmd_type::STATUS_SUCCESS,
                              "操作成功",
                              data);
}

/* ========================= 通知 ========================= */

void AccountHandler::notifyFriendOnlineStatus(int userId, int onlineStatus)
{
    auto conn = _cpool->getConnection();
    if (!conn)
        return;

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT user_id FROM sys_friend_relation WHERE friend_id=%d",
             userId);

    MYSQL_RES *res = conn->query(sql);
    if (!res)
        return;

    nlohmann::json msg;
    msg["cmd"] = json_rpc_protocol::cmd_type::CMD_FRIEND_STATUS_NOTIFY;
    msg["data"]["user_id"] = userId;
    msg["data"]["online_status"] = onlineStatus;

    std::string payload = msg.dump();

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        int fid = atoi(row[0]);
        auto it = _userConnections->find(fid);
        if (it != _userConnections->end())
            it->second->send(payload);
    }

    mysql_free_result(res);
}

void AccountHandler::notifyFriendAvatarChanged(int userId, const std::string &avatarPathOrUrl)
{
    auto conn = _cpool->getConnection();
    if (!conn)
        return;

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT user_id FROM sys_friend_relation WHERE friend_id=%d",
             userId);

    MYSQL_RES *res = conn->query(sql);
    if (!res)
        return;

    nlohmann::json msg;
    msg["cmd"] = json_rpc_protocol::cmd_type::CMD_FRIEND_AVATAR_NOTIFY;
    msg["data"]["user_id"] = userId;
    msg["data"]["avatar"] = avatarPathOrUrl;

    std::string payload = msg.dump();

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        int fid = atoi(row[0]);
        auto it = _userConnections->find(fid);
        if (it != _userConnections->end())
            it->second->send(payload);
    }

    mysql_free_result(res);
}

void AccountHandler::notifyGroupMemberChanged(int groupId,
                                              const std::string &action,
                                              int targetUserId)
{
    auto conn = _cpool->getConnection();
    if (!conn)
        return;

    char sql[512];
    snprintf(sql, sizeof(sql),
             "SELECT user_id FROM sys_group_member WHERE group_id=%d",
             groupId);

    MYSQL_RES *res = conn->query(sql);
    if (!res)
        return;

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        int uid = row[0] ? atoi(row[0]) : 0;

        int selfMuteStatus = 0;
        int selfRole = queryGroupRole(conn.get(), groupId, uid, &selfMuteStatus);

        nlohmann::json msg;
        msg["cmd"] = json_rpc_protocol::cmd_type::CMD_GROUP_MEMBER_NOTIFY;
        msg["data"]["group_id"] = groupId;
        msg["data"]["action"] = action;
        msg["data"]["target_user_id"] = targetUserId;
        msg["data"]["self_role"] = selfRole;
        msg["data"]["self_mute_status"] = selfMuteStatus;

        auto it = _userConnections->find(uid);
        if (it != _userConnections->end())
            it->second->send(msg.dump());
    }
    mysql_free_result(res);

    if (action == "kicked" || action == "dismiss")
    {
        auto it = _userConnections->find(targetUserId);
        if (it != _userConnections->end())
        {
            nlohmann::json msg;
            msg["cmd"] = json_rpc_protocol::cmd_type::CMD_GROUP_MEMBER_NOTIFY;
            msg["data"]["group_id"] = groupId;
            msg["data"]["action"] = action;
            msg["data"]["target_user_id"] = targetUserId;
            msg["data"]["self_role"] = 0;
            msg["data"]["self_mute_status"] = 0;
            it->second->send(msg.dump());
        }
    }
}

void AccountHandler::notifyFriendRequest(int toUserId, const nlohmann::json &requestData)
{
    auto it = _userConnections->find(toUserId);
    if (it == _userConnections->end())
        return;

    nlohmann::json msg;
    msg["cmd"] = json_rpc_protocol::cmd_type::CMD_FRIEND_REQUEST_NOTIFY;
    msg["data"] = requestData;
    it->second->send(msg.dump());
}

void AccountHandler::notifyFriendRequestResult(int toUserId, const nlohmann::json &resultData)
{
    auto it = _userConnections->find(toUserId);
    if (it == _userConnections->end())
        return;

    nlohmann::json msg;
    msg["cmd"] = json_rpc_protocol::cmd_type::CMD_FRIEND_REQUEST_RESULT_NOTIFY;
    msg["data"] = resultData;
    it->second->send(msg.dump());
}

void AccountHandler::notifyGroupInvite(int toUserId, const nlohmann::json &inviteData)
{
    auto it = _userConnections->find(toUserId);
    if (it == _userConnections->end())
        return;

    nlohmann::json msg;
    msg["cmd"] = json_rpc_protocol::cmd_type::CMD_GROUP_INVITE_NOTIFY;
    msg["data"] = inviteData;
    it->second->send(msg.dump());
}

void AccountHandler::notifyGroupInviteResult(int toUserId, const nlohmann::json &resultData)
{
    auto it = _userConnections->find(toUserId);
    if (it == _userConnections->end())
        return;

    nlohmann::json msg;
    msg["cmd"] = json_rpc_protocol::cmd_type::CMD_GROUP_INVITE_RESULT_NOTIFY;
    msg["data"] = resultData;
    it->second->send(msg.dump());
}

std::string AccountHandler::handleDeleteFriend(nlohmann::json j, int userId)
{
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_DELETE_FRIEND_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    if (userId <= 0)
        return retMessage(cmd_type::CMD_DELETE_FRIEND_RES,
                          cmd_type::STATUS_NOT_LOGIN,
                          "请先登录");

    std::string targetAccount = j.value("target_account", "");
    if (targetAccount.empty())
        return retMessage(cmd_type::CMD_DELETE_FRIEND_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "目标账号不能为空");

    int targetUserId = 0;
    std::string targetNickname, targetAvatar;
    if (!queryUserByAccount(conn.get(), targetAccount, targetUserId, targetNickname, targetAvatar))
        return retMessage(cmd_type::CMD_DELETE_FRIEND_RES,
                          cmd_type::STATUS_ACCOUNT_NOT_EXISTS,
                          "目标账号不存在");

    if (targetUserId == userId)
        return retMessage(cmd_type::CMD_DELETE_FRIEND_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "不能删除自己");

    if (!isFriend(conn.get(), userId, targetUserId))
        return retMessage(cmd_type::CMD_DELETE_FRIEND_RES,
                          cmd_type::STATUS_FAILED,
                          "对方不是你的好友");

    char sql[2048];
    long long tmpId = 0;

    snprintf(sql, sizeof(sql),
             "DELETE FROM sys_friend_relation "
             "WHERE (user_id=%d AND friend_id=%d) "
             "   OR (user_id=%d AND friend_id=%d)",
             userId, targetUserId, targetUserId, userId);

    if (!conn->update(sql, tmpId))
        return retMessage(cmd_type::CMD_DELETE_FRIEND_RES,
                          cmd_type::STATUS_FAILED,
                          "删除好友失败");

    std::string selfAccount, selfNickname, selfAvatar;
    int selfStatus = 0;
    queryUserBasic(conn.get(), userId, selfAccount, selfNickname, selfAvatar, &selfStatus);

    nlohmann::json notifyData;
    notifyData["user_id"] = userId;
    notifyData["account"] = selfAccount;
    notifyData["nickname"] = selfNickname;
    notifyData["avatar"] = selfAvatar;

    notifyFriendDeleted(targetUserId, notifyData);

    nlohmann::json data;
    data["friend_user_id"] = targetUserId;
    data["account"] = targetAccount;

    return retMessageWithData(cmd_type::CMD_DELETE_FRIEND_RES,
                              cmd_type::STATUS_SUCCESS,
                              "删除好友成功",
                              data);
}

void AccountHandler::notifyFriendDeleted(int toUserId, const nlohmann::json &data)
{
    auto it = _userConnections->find(toUserId);
    if (it == _userConnections->end())
        return;

    nlohmann::json msg;
    msg["cmd"] = json_rpc_protocol::cmd_type::CMD_FRIEND_DELETED_NOTIFY;
    msg["data"] = data;
    it->second->send(msg.dump());
}

std::string AccountHandler::handleRegister(nlohmann::json j)
{
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_REGISTER_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    std::string password = j.value("password", "");
    std::string nickname = j.value("nickname", "");

    if (password.empty())
        return retMessage(cmd_type::CMD_REGISTER_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "密码不能为空");

    std::string escPassword = escapeSql(conn.get(), password);
    std::string escNickname = escapeSql(conn.get(), nickname);

    char sql[1024];
    long long insertId = 0;

    // 先插入一条记录，account 先留空占位
    snprintf(sql, sizeof(sql),
             "INSERT INTO sys_user(account,password_hash,nickname,avatar,register_time,status) "
             "VALUES('','%s','%s','',NOW(),0)",
             escPassword.c_str(), escNickname.c_str());

    if (!conn->update(sql, insertId))
        return retMessage(cmd_type::CMD_REGISTER_RES,
                          cmd_type::STATUS_FAILED,
                          "注册失败");

    // 根据 user_id 生成唯一纯数字账号
    long long accountNum = 10000000LL + insertId;
    std::string account = std::to_string(accountNum);
    std::string escAccount = escapeSql(conn.get(), account);

    long long tmpId = 0;
    snprintf(sql, sizeof(sql),
             "UPDATE sys_user SET account='%s' WHERE user_id=%lld",
             escAccount.c_str(), insertId);

    if (!conn->update(sql, tmpId))
        return retMessage(cmd_type::CMD_REGISTER_RES,
                          cmd_type::STATUS_FAILED,
                          "账号生成失败");

    nlohmann::json data;
    data["user_id"] = insertId;
    data["account"] = account;
    data["nickname"] = nickname;

    return retMessageWithData(cmd_type::CMD_REGISTER_RES,
                              cmd_type::STATUS_SUCCESS,
                              "注册成功",
                              data);
}

std::string AccountHandler::handleResetPassword(nlohmann::json j)
{
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_RESET_PW_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    std::string account = j.value("account", "");
    std::string newPassword = j.value("new_password", "");

    if (account.empty() || newPassword.empty())
        return retMessage(cmd_type::CMD_RESET_PW_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "账号或新密码不能为空");

    if (!isNumericAccount(account))
        return retMessage(cmd_type::CMD_RESET_PW_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "账号必须是6到13位纯数字");

    std::string escAccount = escapeSql(conn.get(), account);
    std::string escPassword = escapeSql(conn.get(), newPassword);

    char sql[1024];
    snprintf(sql, sizeof(sql),
             "SELECT user_id FROM sys_user WHERE account='%s'",
             escAccount.c_str());

    MYSQL_RES *res = conn->query(sql);
    if (!res)
        return retMessage(cmd_type::CMD_RESET_PW_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库查询失败");

    if (mysql_num_rows(res) == 0)
    {
        mysql_free_result(res);
        return retMessage(cmd_type::CMD_RESET_PW_RES,
                          cmd_type::STATUS_ACCOUNT_NOT_EXISTS,
                          "账号不存在");
    }
    mysql_free_result(res);

    long long tmpId = 0;
    snprintf(sql, sizeof(sql),
             "UPDATE sys_user SET password_hash='%s' WHERE account='%s'",
             escPassword.c_str(), escAccount.c_str());

    if (!conn->update(sql, tmpId))
        return retMessage(cmd_type::CMD_RESET_PW_RES,
                          cmd_type::STATUS_FAILED,
                          "密码重置失败");

    return retMessage(cmd_type::CMD_RESET_PW_RES,
                      cmd_type::STATUS_SUCCESS,
                      "密码重置成功");
}