#include "ChatMessageHandler.h"
#include "ConnectionPool.h"
#include "Connection.h"
#include "protocol.h"
#include "MysqlConnection.h"

#include <mysql/mysql.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace
{
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

    // 这里改成你的 Ubuntu 虚拟机实际 IP + python3 http.server 端口
    const std::string kAvatarHttpBase = "http://192.168.137.144:8080";

    std::string normalizeAvatarForClient(const std::string &dbValue)
    {
        if (dbValue.empty())
            return "";

        if (dbValue.rfind("http://", 0) == 0 || dbValue.rfind("https://", 0) == 0)
            return dbValue;

        if (dbValue.rfind("/static/", 0) == 0)
            return kAvatarHttpBase + dbValue;

        if (dbValue.rfind("static/", 0) == 0)
            return kAvatarHttpBase + "/" + dbValue;

        return dbValue;
    }

    std::string nowDateTime()
    {
        char buf[32] = {0};
        std::time_t now = std::time(nullptr);
        std::tm *lt = std::localtime(&now);
        if (lt)
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", lt);
        return buf;
    }
}

MessageHandler::MessageHandler(std::unordered_map<int, Connection *> *userConnections)
    : _cpool(ConnectionPool::getConnectionPool()), _userConnections(userConnections)
{
}

std::string MessageHandler::retMessage(json_rpc_protocol::cmd_type cmd,
                                       json_rpc_protocol::cmd_type status,
                                       const char *message)
{
    nlohmann::json response;
    response["cmd"] = cmd;
    response["status"] = status;
    response["message"] = message;
    return response.dump();
}

std::string MessageHandler::retMessageWithData(json_rpc_protocol::cmd_type cmd,
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

std::string MessageHandler::handleSendPrivateMessage(nlohmann::json j,
                                                     int senderId,
                                                     std::unordered_map<int, Connection *> *currentConnection)
{
    _userConnections = currentConnection;
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_SEND_PRIVATE_MSG_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    std::string targetAccount = j.value("target_account", "");
    int msgType = j.value("msg_type", 1);
    std::string content = j.value("content", "");
    std::string filename = j.value("filename", "");

    if (senderId <= 0)
        return retMessage(cmd_type::CMD_SEND_PRIVATE_MSG_RES,
                          cmd_type::STATUS_NOT_LOGIN,
                          "请先登录");

    if (targetAccount.empty() || content.empty())
        return retMessage(cmd_type::CMD_SEND_PRIVATE_MSG_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "参数不完整");

    std::string escTargetAccount = escapeSql(conn.get(), targetAccount);
    std::string escContent = escapeSql(conn.get(), content);
    std::string escFilename = escapeSql(conn.get(), filename);

    char sql[8192];

    snprintf(sql, sizeof(sql),
             "SELECT user_id FROM sys_user WHERE account='%s'",
             escTargetAccount.c_str());

    MYSQL_RES *res = conn->query(sql);
    if (!res)
        return retMessage(cmd_type::CMD_SEND_PRIVATE_MSG_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库查询失败");

    if (mysql_num_rows(res) == 0)
    {
        mysql_free_result(res);
        return retMessage(cmd_type::CMD_SEND_PRIVATE_MSG_RES,
                          cmd_type::STATUS_ACCOUNT_NOT_EXISTS,
                          "目标账号不存在");
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    int receiverId = row[0] ? atoi(row[0]) : 0;
    mysql_free_result(res);

    long long msgId = 0;
    snprintf(sql, sizeof(sql),
             "INSERT INTO sys_message(sender_id, receiver_id, group_id, chat_type, msg_type, content, filename, send_time, msg_status) "
             "VALUES(%d, %d, NULL, 'friend', %d, '%s', '%s', NOW(), 0)",
             senderId, receiverId, msgType, escContent.c_str(), escFilename.c_str());

    if (!conn->update(sql, msgId))
        return retMessage(cmd_type::CMD_SEND_PRIVATE_MSG_RES,
                          cmd_type::STATUS_FAILED,
                          "消息入库失败");

    nlohmann::json data;
    data["msg_id"] = msgId;

    auto it = _userConnections->find(receiverId);
    if (it != _userConnections->end() && it->second)
    {
        snprintf(sql, sizeof(sql),
                 "SELECT nickname, avatar FROM sys_user WHERE user_id=%d",
                 senderId);
        res = conn->query(sql);

        std::string nickname, avatar;
        if (res && mysql_num_rows(res) > 0)
        {
            row = mysql_fetch_row(res);
            nickname = safeString(row[0]);
            avatar = normalizeAvatarForClient(safeString(row[1]));
        }
        if (res)
            mysql_free_result(res);

        nlohmann::json notify;
        notify["cmd"] = cmd_type::CMD_PRIVATE_MSG_NOTIFY;
        notify["data"]["msg_id"] = msgId;
        notify["data"]["from_user_id"] = senderId;
        notify["data"]["from_nickname"] = nickname;
        notify["data"]["from_avatar"] = avatar;
        notify["data"]["msg_type"] = msgType;
        notify["data"]["content"] = content;
        notify["data"]["filename"] = filename;
        notify["data"]["timestamp"] = nowDateTime();

        it->second->send(notify.dump());
    }
    else
    {
        snprintf(sql, sizeof(sql),
                 "INSERT INTO sys_offline_message(sender_id, receiver_id, content, send_time, read_flag, msg_type, filename) "
                 "VALUES(%d, %d, '%s', NOW(), 0, %d, '%s')",
                 senderId, receiverId, escContent.c_str(), msgType, escFilename.c_str());
        conn->update(sql, msgId);
    }

    return retMessageWithData(cmd_type::CMD_SEND_PRIVATE_MSG_RES,
                              cmd_type::STATUS_SUCCESS,
                              "发送成功",
                              data);
}

std::string MessageHandler::handleGetPrivateHistory(nlohmann::json j, int userId)
{
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_GET_PRIVATE_HISTORY_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    if (userId <= 0)
        return retMessage(cmd_type::CMD_GET_PRIVATE_HISTORY_RES,
                          cmd_type::STATUS_NOT_LOGIN,
                          "请先登录");

    int friendUserId = j.value("friend_user_id", 0);
    int limit = j.value("limit", 50);

    if (friendUserId <= 0)
        return retMessage(cmd_type::CMD_GET_PRIVATE_HISTORY_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "好友ID无效");

    if (limit <= 0)
        limit = 50;

    char sql[8192];
    snprintf(sql, sizeof(sql),
             "SELECT m.msg_id, m.sender_id, m.receiver_id, m.msg_type, m.content, m.filename, m.send_time, "
             "u.nickname, u.avatar "
             "FROM sys_message m "
             "JOIN sys_user u ON m.sender_id = u.user_id "
             "WHERE m.chat_type='friend' "
             "AND ((m.sender_id=%d AND m.receiver_id=%d) OR (m.sender_id=%d AND m.receiver_id=%d)) "
             "ORDER BY m.send_time ASC, m.msg_id ASC "
             "LIMIT %d",
             userId, friendUserId, friendUserId, userId, limit);

    MYSQL_RES *res = conn->query(sql);
    nlohmann::json arr = nlohmann::json::array();

    if (res)
    {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)))
        {
            nlohmann::json m;
            m["msg_id"] = row[0] ? atoll(row[0]) : 0;
            m["sender_id"] = row[1] ? atoi(row[1]) : 0;
            m["receiver_id"] = row[2] ? atoi(row[2]) : 0;
            m["msg_type"] = row[3] ? atoi(row[3]) : 1;
            m["content"] = safeString(row[4]);
            m["filename"] = safeString(row[5]);
            m["send_time"] = safeString(row[6]);
            m["sender_name"] = safeString(row[7]);
            m["sender_avatar"] = normalizeAvatarForClient(safeString(row[8]));
            m["target_account"] = "";
            arr.push_back(m);
        }
        mysql_free_result(res);
    }

    nlohmann::json data;
    data["friend_user_id"] = friendUserId;
    data["messages"] = arr;

    return retMessageWithData(cmd_type::CMD_GET_PRIVATE_HISTORY_RES,
                              cmd_type::STATUS_SUCCESS,
                              "获取历史成功",
                              data);
}

std::string MessageHandler::handleSendGroupMessage(nlohmann::json j,
                                                   int senderId,
                                                   std::unordered_map<int, Connection *> *currentConnection)
{
    _userConnections = currentConnection;
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_SEND_GROUP_MSG_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    if (senderId <= 0)
        return retMessage(cmd_type::CMD_SEND_GROUP_MSG_RES,
                          cmd_type::STATUS_NOT_LOGIN,
                          "请先登录");

    int groupId = j.value("group_id", 0);
    int msgType = j.value("msg_type", 1);
    std::string content = j.value("content", "");
    std::string filename = j.value("filename", "");

    if (groupId <= 0 || content.empty())
        return retMessage(cmd_type::CMD_SEND_GROUP_MSG_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "参数不完整");

    std::string escContent = escapeSql(conn.get(), content);
    std::string escFilename = escapeSql(conn.get(), filename);

    char sql[8192];
    snprintf(sql, sizeof(sql),
             "SELECT role, mute_status FROM sys_group_member WHERE group_id=%d AND user_id=%d",
             groupId, senderId);

    MYSQL_RES *res = conn->query(sql);
    if (!res)
        return retMessage(cmd_type::CMD_SEND_GROUP_MSG_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库查询失败");

    if (mysql_num_rows(res) == 0)
    {
        mysql_free_result(res);
        return retMessage(cmd_type::CMD_SEND_GROUP_MSG_RES,
                          cmd_type::STATUS_NO_PERMISSION,
                          "你不在该群中");
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    int muteStatus = row[1] ? atoi(row[1]) : 0;
    mysql_free_result(res);

    if (muteStatus == 1)
        return retMessage(cmd_type::CMD_SEND_GROUP_MSG_RES,
                          cmd_type::STATUS_USER_MUTED,
                          "你已被禁言");

    long long msgId = 0;
    snprintf(sql, sizeof(sql),
             "INSERT INTO sys_message(sender_id, receiver_id, group_id, chat_type, msg_type, content, filename, send_time, msg_status) "
             "VALUES(%d, NULL, %d, 'group', %d, '%s', '%s', NOW(), 0)",
             senderId, groupId, msgType, escContent.c_str(), escFilename.c_str());

    if (!conn->update(sql, msgId))
        return retMessage(cmd_type::CMD_SEND_GROUP_MSG_RES,
                          cmd_type::STATUS_FAILED,
                          "群消息入库失败");

    std::string nickname, avatar;
    snprintf(sql, sizeof(sql),
             "SELECT nickname, avatar FROM sys_user WHERE user_id=%d",
             senderId);

    res = conn->query(sql);
    if (res && mysql_num_rows(res) > 0)
    {
        row = mysql_fetch_row(res);
        nickname = safeString(row[0]);
        avatar = normalizeAvatarForClient(safeString(row[1]));
    }
    if (res)
        mysql_free_result(res);

    snprintf(sql, sizeof(sql),
             "SELECT user_id FROM sys_group_member WHERE group_id=%d AND user_id<>%d",
             groupId, senderId);
    res = conn->query(sql);

    if (res)
    {
        nlohmann::json notify;
        notify["cmd"] = cmd_type::CMD_GROUP_MSG_NOTIFY;
        notify["data"]["msg_id"] = msgId;
        notify["data"]["group_id"] = groupId;
        notify["data"]["from_user_id"] = senderId;
        notify["data"]["from_nickname"] = nickname;
        notify["data"]["from_avatar"] = avatar;
        notify["data"]["msg_type"] = msgType;
        notify["data"]["content"] = content;
        notify["data"]["filename"] = filename;
        notify["data"]["timestamp"] = nowDateTime();

        std::string payload = notify.dump();

        while ((row = mysql_fetch_row(res)))
        {
            int uid = row[0] ? atoi(row[0]) : 0;
            auto it = _userConnections->find(uid);
            if (it != _userConnections->end() && it->second)
                it->second->send(payload);
        }
        mysql_free_result(res);
    }

    nlohmann::json data;
    data["msg_id"] = msgId;

    return retMessageWithData(cmd_type::CMD_SEND_GROUP_MSG_RES,
                              cmd_type::STATUS_SUCCESS,
                              "发送成功",
                              data);
}

std::string MessageHandler::handleGetGroupHistory(nlohmann::json j, int userId)
{
    using json_rpc_protocol::cmd_type;

    auto conn = _cpool->getConnection();
    if (!conn)
        return retMessage(cmd_type::CMD_GET_GROUP_HISTORY_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库连接失败");

    if (userId <= 0)
        return retMessage(cmd_type::CMD_GET_GROUP_HISTORY_RES,
                          cmd_type::STATUS_NOT_LOGIN,
                          "请先登录");

    int groupId = j.value("group_id", 0);
    int limit = j.value("limit", 50);

    if (groupId <= 0)
        return retMessage(cmd_type::CMD_GET_GROUP_HISTORY_RES,
                          cmd_type::STATUS_INVALID_PARAMS,
                          "群ID无效");

    if (limit <= 0)
        limit = 50;

    char sql[8192];
    snprintf(sql, sizeof(sql),
             "SELECT member_id FROM sys_group_member WHERE group_id=%d AND user_id=%d",
             groupId, userId);

    MYSQL_RES *res = conn->query(sql);
    if (!res)
        return retMessage(cmd_type::CMD_GET_GROUP_HISTORY_RES,
                          cmd_type::STATUS_DB_CONNECT_FAILED,
                          "数据库查询失败");

    if (mysql_num_rows(res) == 0)
    {
        mysql_free_result(res);
        return retMessage(cmd_type::CMD_GET_GROUP_HISTORY_RES,
                          cmd_type::STATUS_NO_PERMISSION,
                          "你不在该群中");
    }
    mysql_free_result(res);

    snprintf(sql, sizeof(sql),
             "SELECT m.msg_id, m.sender_id, m.group_id, m.msg_type, m.content, m.filename, m.send_time, "
             "u.nickname, u.avatar "
             "FROM sys_message m "
             "JOIN sys_user u ON m.sender_id = u.user_id "
             "WHERE m.chat_type='group' AND m.group_id=%d "
             "ORDER BY m.send_time ASC, m.msg_id ASC "
             "LIMIT %d",
             groupId, limit);

    res = conn->query(sql);
    nlohmann::json arr = nlohmann::json::array();

    if (res)
    {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)))
        {
            nlohmann::json m;
            m["msg_id"] = row[0] ? atoll(row[0]) : 0;
            m["sender_id"] = row[1] ? atoi(row[1]) : 0;
            m["group_id"] = row[2] ? atoi(row[2]) : 0;
            m["msg_type"] = row[3] ? atoi(row[3]) : 1;
            m["content"] = safeString(row[4]);
            m["filename"] = safeString(row[5]);
            m["send_time"] = safeString(row[6]);
            m["sender_name"] = safeString(row[7]);
            m["sender_avatar"] = normalizeAvatarForClient(safeString(row[8]));
            arr.push_back(m);
        }
        mysql_free_result(res);
    }

    nlohmann::json data;
    data["group_id"] = groupId;
    data["messages"] = arr;

    return retMessageWithData(cmd_type::CMD_GET_GROUP_HISTORY_RES,
                              cmd_type::STATUS_SUCCESS,
                              "获取群历史成功",
                              data);
}