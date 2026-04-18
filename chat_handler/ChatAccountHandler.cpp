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
    // 这里改成你的 Ubuntu 虚拟机实际 IP + python3 http.server 端口
    // 例如在项目根目录执行：
    // python3 -m http.server 8080
    // 那么 ./static/avatar/user_3.jpg 就能通过
    // http://192.168.137.144:8080/static/avatar/user_3.jpg 访问
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

    bool isAllowedImageExt(const std::string &ext)
    {
        return ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "bmp" || ext == "webp";
    }

    // 数据库存相对路径，发给客户端时转成 HTTP URL
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

        // 数据库存的是 /static/avatar/xxx.jpg 时，返回 http://ip:8080/static/avatar/xxx.jpg
        if (dbValue.rfind("/static/", 0) == 0 || dbValue.rfind("static/", 0) == 0)
            return buildAvatarUrl(dbValue);

        // 兼容已存在的 http 链接
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

/* ========================= 工具函数 ========================= */

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

/* ========================= 注册 ========================= */

std::string AccountHandler::handleRegister(nlohmann::json j)
{
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_REGISTER_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    std::string account = j.value("account", "");
    std::string password = j.value("password", "");
    std::string nickname = j.value("nickname", "");

    if (account.empty() || password.empty())
        return retMessage(cmd_type::CMD_REGISTER_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "账号或密码不能为空");

    std::string escAccount = escapeSql(conn.get(), account);
    std::string escPassword = escapeSql(conn.get(), password);
    std::string escNickname = escapeSql(conn.get(), nickname);

    char sql[1024];

    snprintf(sql, sizeof(sql),
             "SELECT user_id FROM sys_user WHERE account='%s'",
             escAccount.c_str());

    MYSQL_RES *res = conn->query(sql);
    if (res && mysql_num_rows(res) > 0)
    {
        mysql_free_result(res);
        return retMessage(cmd_type::CMD_REGISTER_RES,
                          cmd_type::STATUS_ACCOUNT_EXISTS,
                          "账号已存在");
    }
    if (res)
        mysql_free_result(res);

    long long insertId = 0;

    snprintf(sql, sizeof(sql),
             "INSERT INTO sys_user(account,password_hash,nickname,avatar,register_time,status) "
             "VALUES('%s','%s','%s','',NOW(),0)",
             escAccount.c_str(), escPassword.c_str(), escNickname.c_str());

    if (!conn->update(sql, insertId))
        return retMessage(cmd_type::CMD_REGISTER_RES,
                          cmd_type::STATUS_FAILED,
                          "注册失败");

    nlohmann::json data;
    data["user_id"] = insertId;

    return retMessageWithData(cmd_type::CMD_REGISTER_RES,
                              cmd_type::STATUS_SUCCESS,
                              "注册成功",
                              data);
}

/* ========================= 登录 ========================= */

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

    std::string escAccount = escapeSql(conn.get(), account);

    char sql[4096];
    snprintf(sql, sizeof(sql),
             "SELECT user_id, account, password_hash, nickname, avatar, status "
             "FROM sys_user WHERE account='%s'",
             escAccount.c_str());

    MYSQL_RES *res = conn->query(sql);
    if (!res)
    {
        return retMessage(cmd_type::CMD_LOGIN_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库查询失败");
    }

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
             "SELECT g.group_id, g.group_name, gm.role, gm.mute_status "
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
            g["role"] = row[2] ? atoi(row[2]) : 0;
            g["mute_status"] = row[3] ? atoi(row[3]) : 0;
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

/* ========================= 修改头像 ========================= */

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

    // 数据库只保存路径
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

    // 通知时发给客户端可直接访问的 http URL
    notifyFriendAvatarChanged(userId, buildAvatarUrl(avatarPathInDb));

    nlohmann::json data;
    data["avatar"] = buildAvatarUrl(avatarPathInDb);

    return retMessageWithData(cmd_type::CMD_UPDATE_AVATAR_RES,
                              cmd_type::STATUS_SUCCESS,
                              "更新成功",
                              data);
}

/* ========================= 创建群 ========================= */

std::string AccountHandler::handleCreateGroup(nlohmann::json j, int userId)
{
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_CREATE_GROUP_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    std::string groupName = j.value("group_name", "");
    if (groupName.empty())
        return retMessage(cmd_type::CMD_CREATE_GROUP_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "群名不能为空");

    std::string escGroupName = escapeSql(conn.get(), groupName);

    char sql[1024];
    long long id = 0;

    snprintf(sql, sizeof(sql),
             "INSERT INTO sys_group(group_name,creator_id,create_time,notice) "
             "VALUES('%s',%d,NOW(),'')",
             escGroupName.c_str(), userId);

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

    return retMessageWithData(cmd_type::CMD_CREATE_GROUP_RES,
                              cmd_type::STATUS_SUCCESS,
                              "创建成功",
                              data);
}

/* ========================= 加入群 ========================= */

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

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT group_id FROM sys_group WHERE group_id=%d",
             groupId);

    MYSQL_RES *res = conn->query(sql);
    if (!res || mysql_num_rows(res) == 0)
    {
        if (res)
            mysql_free_result(res);
        return retMessage(cmd_type::CMD_JOIN_GROUP_RES,
                          cmd_type::STATUS_GROUP_NOT_EXISTS,
                          "群不存在");
    }
    mysql_free_result(res);

    snprintf(sql, sizeof(sql),
             "SELECT member_id FROM sys_group_member WHERE group_id=%d AND user_id=%d",
             groupId, userId);

    res = conn->query(sql);
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

    snprintf(sql, sizeof(sql),
             "SELECT group_name FROM sys_group WHERE group_id=%d",
             groupId);

    res = conn->query(sql);
    if (res && mysql_num_rows(res) > 0)
    {
        MYSQL_ROW row = mysql_fetch_row(res);
        data["group_name"] = safeString(row[0]);
    }
    if (res)
        mysql_free_result(res);

    notifyGroupMemberChanged(groupId, "join", userId, 0, 0);

    return retMessageWithData(cmd_type::CMD_JOIN_GROUP_RES,
                              cmd_type::STATUS_SUCCESS,
                              "加入成功",
                              data);
}

/* ========================= 群管理（简化版） ========================= */

std::string AccountHandler::handleGroupManage(nlohmann::json j, int userId)
{
    using json_rpc_protocol::cmd_type;

    int groupId = j.value("group_id", 0);
    std::string action = j.value("action", "");

    if (groupId <= 0)
        return retMessage(cmd_type::CMD_GROUP_MANAGE_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "参数错误");

    notifyGroupMemberChanged(groupId, action, userId, 0, 0);

    nlohmann::json data;
    data["group_id"] = groupId;
    data["self_role"] = 0;
    data["self_mute_status"] = 0;

    return retMessageWithData(cmd_type::CMD_GROUP_MANAGE_RES,
                              cmd_type::STATUS_SUCCESS,
                              "操作成功",
                              data);
}

/* ========================= 通知函数 ========================= */

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
                                              int targetUserId,
                                              int selfRole,
                                              int selfMuteStatus)
{
    auto conn = _cpool->getConnection();
    if (!conn)
        return;

    char sql[256];
    snprintf(sql, sizeof(sql),
             "SELECT user_id FROM sys_group_member WHERE group_id=%d",
             groupId);

    MYSQL_RES *res = conn->query(sql);
    if (!res)
        return;

    nlohmann::json msg;
    msg["cmd"] = json_rpc_protocol::cmd_type::CMD_GROUP_MEMBER_NOTIFY;
    msg["data"]["group_id"] = groupId;
    msg["data"]["action"] = action;
    msg["data"]["target_user_id"] = targetUserId;
    msg["data"]["self_role"] = selfRole;
    msg["data"]["self_mute_status"] = selfMuteStatus;

    std::string payload = msg.dump();

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)))
    {
        int uid = atoi(row[0]);
        auto it = _userConnections->find(uid);
        if (it != _userConnections->end())
            it->second->send(payload);
    }

    mysql_free_result(res);
}