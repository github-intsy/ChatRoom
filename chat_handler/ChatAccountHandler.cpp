#include "ChatAccountHandler.h"
#include "ConnectionPool.h"
#include "protocol.h"
#include "MysqlConnection.h"
#include <mysql/mysql.h>
AccountHandler::AccountHandler(std::unordered_map<int, Connection *> &userConnections)
    : _cpool(ConnectionPool::getConnectionPool()), _userConnections(userConnections)
{
}

std::string AccountHandler::handleRegister(nlohmann::json_abi_v3_12_0::json j)
{
    return "";
}

std::string AccountHandler::handleLogin(nlohmann::json_abi_v3_12_0::json j,
                                        std::unordered_map<int, const Connection *> &currentConnection, const Connection *tcpConnection)
{
    /*
        连接数据库获取数据表
        查找账号是否在数据库指定表中
            不存在返回json
            存在判断密码对应账号
            全部正确调用对应材料数据返回json
    */
    nlohmann::json response;
    using json_rpc_protocol::cmd_type;
    try
    {
        std::string account = j["account"];   // 账号
        std::string password = j["password"]; // 密码
        // 从连接池获取连接
        auto conn = _cpool->getConnection();
        if (!conn)
            return retMessage(cmd_type::CMD_LOGIN_RES, cmd_type::STATUS_DB_CONNECT_FAILED, "数据库连接失败");
        // 构建查询sql
        char sql[256];
        snprintf(sql, sizeof(sql),
                 "select user_id, account, password_hash, nickname, status from sys_user where account = '%s'",
                 account.c_str());
        // 执行查询
        MYSQL_RES *result = conn->query(sql);
        if (!result)
            return retMessage(cmd_type::CMD_LOGIN_RES, cmd_type::STATUS_ACCOUNT_NOT_EXISTS, "该账号不存在");
        // 获得结果行数
        int rowCount = mysql_num_rows(result);
        if (0 == rowCount)
        {
            mysql_free_result(result);
            return retMessage(cmd_type::CMD_LOGIN_RES, cmd_type::STATUS_ACCOUNT_NOT_EXISTS, "该账号不存在");
        }
        // 提取数据
        MYSQL_ROW row = mysql_fetch_row(result);
        if (nullptr == row)
        {
            mysql_free_result(result);
            return retMessage(cmd_type::CMD_LOGIN_RES, cmd_type::STATUS_FAILED, "数据读取失败");
        }
        // row 索引对应 SELECT 字段顺序
        // row[0] = user_id bigint
        // row[1] = account varchar13
        // row[2] = password_hash varchar255
        // row[3] = nickname    varchar100
        // row[4] = status      tinyint
        std::string db_passwd = row[2] ? row[2] : "";
        std::string nickname = row[3] ? row[3] : "";
        int status = row[4] ? atoi(row[4]) : 0;
        int user_id = row[0] ? atoi(row[0]) : 0;
        // 检查密码
        if (db_passwd != password)
        {
            mysql_free_result(result);
            return retMessage(cmd_type::CMD_LOGIN_RES, cmd_type::STATUS_PASSWORD_ERROR, "密码错误");
        }
        // 检查账号状态
        if (0 == status)
        {
            // 更新为在线状态
            std::string updateSql = "update sys_user set status = 1 where user_id = ";
            updateSql += std::to_string(user_id);
            if (conn->update(updateSql)) // 完成登录逻辑，更新userconnection
            {
                int userAccount = atoi(row[1]);
                currentConnection[userAccount] = tcpConnection;
            }
            else
            {
                printf("sql语句更新失败\n");
                return retMessage(cmd_type::CMD_LOGIN_RES, cmd_type::STATUS_INTERNAL_ERROR, "登录失败");
            }
        }
        else
        {
            // 用户已经存在一个维护中的链接，等待后续增加处理
        }

        // 登录成功
        return retMessage(cmd_type::CMD_LOGIN_RES, cmd_type::STATUS_SUCCESS, "登陆成功");
        // 后续登陆成功传回给客户端需要加载的东西
    }
    catch (const std::exception &e)
    {
        std::string tmp = "服务器错误：";
        tmp += e.what();
        return retMessage(cmd_type::CMD_LOGIN_RES, cmd_type::STATUS_FAILED, tmp.c_str());
    }
    _cpool->getConnection()->query("select account from sys_user");
    return retMessage(cmd_type::CMD_LOGIN_RES, cmd_type::STATUS_FAILED, "服务器未知错误");
}

std::string AccountHandler::handleResetPassword(nlohmann::json_abi_v3_12_0::json j)
{
    return "";
}

std::string AccountHandler::retMessage(json_rpc_protocol::cmd_type cmd,
                                       json_rpc_protocol::cmd_type status, const char *message)
{
    nlohmann::json response;
    response["cmd"] = cmd;
    response["status"] = status;
    response["message"] = message;
    return response.dump();
}