#pragma once
#include <json.hpp>

/*
status,含义,适用命令,说明（message 建议文案）
0,成功,所有响应,操作成功
1,通用失败,所有,兼容旧版，message 再说明原因
2,账号已存在,注册,注册时账号重复
3,账号不存在,登录、重置密码,账号在数据库中不存在
4,密码错误,登录,密码不匹配
5,数据库连接失败,所有,MySQL 连接/查询异常
6,参数无效,所有,缺少字段、格式错误、长度不符等
7,服务器内部错误,所有,其他未知异常（如 hash 失败等）
8,未知命令,所有,服务端收到未定义的 cmd
*/
namespace json_rpc_protocol
{
    enum class cmd_type
    {
        // 状态码
        STATUS_SUCCESS = 0,
        STATUS_FAILED = 1, // 通用失败
        STATUS_ACCOUNT_EXISTS = 2,
        STATUS_ACCOUNT_NOT_EXISTS = 3,
        STATUS_PASSWORD_ERROR = 4,
        STATUS_DB_CONNECT_FAILED = 5,
        STATUS_INVALID_PARAMS = 6,
        STATUS_INTERNAL_ERROR = 7,
        STATUS_UNKNOWN_CMD = 8,

                // 请求
        CMD_REGISTER_REQ = 1001, // 注册请求
        CMD_LOGIN_REQ,           // 登录请求
        CMD_RESET_PW_REQ,        // 重置密码请求

        // 响应
        CMD_REGISTER_RES = 2001, // 注册响应
        CMD_LOGIN_RES,           // 登录响应
        CMD_RESET_PW_RES         // 重置密码响应

    };
}