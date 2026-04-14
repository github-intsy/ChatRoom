#include "MysqlConnection.h"

MysqlConnection::MysqlConnection()
{
    _conn = mysql_init(nullptr); // 初始化数据库连接
}

MysqlConnection::~MysqlConnection()
{
    // 释放数据库连接资源
    if (_conn != nullptr)
        mysql_close(_conn);
}

bool MysqlConnection::connect(const std::string &ip, const uint16_t &port,
                              const std::string &username, const std::string &password,
                              const std::string &dbname)
{
    // 连接数据库
    MYSQL *p = mysql_real_connect(_conn, ip.c_str(), username.c_str(),
                                  password.c_str(), dbname.c_str(), port, nullptr, 0);
    return p != nullptr;
}

bool MysqlConnection::update(const std::string &sql)
{
    // 更新操作insert delete update
    if (mysql_query(_conn, sql.c_str()))
    {
        printf("更新失败：%s\n", sql.c_str());
        return false;
    }
    return true;
}

MYSQL_RES *MysqlConnection::query(const std::string &sql)
{
    // 查询操作
    if (mysql_query(_conn, sql.c_str()))
    {
        printf("查询失败: %s\n%s", sql.c_str(), mysql_error(_conn));

        return nullptr;
    }
    return mysql_store_result(_conn);
}

void MysqlConnection::refreshAliveTime()
{
    _aliveTime = clock();
}

clock_t MysqlConnection::getAliveTime() const
{
    return clock() - _aliveTime;
}