#pragma once
#include <mysql/mysql.h>
#include <string>
class MysqlConnection
{
public:
    MysqlConnection();
    ~MysqlConnection();
    // 连接数据库
    bool connect(const std::string &ip, const uint16_t &port,
                 const std::string &username, const std::string &password, const std::string &dbname);
    // 更新操作insert delete update
    bool update(const std::string &sql, long long & insertId);
    // 查询操作select
    MYSQL_RES *query(const std::string &sql);
    void refreshAliveTime();
    clock_t getAliveTime() const;
    MYSQL* getRawConnection();
private:
    MYSQL *_conn;       // 表示和Mysql server的一条连接
    clock_t _aliveTime; // 记录进入空闲状态后的起始存活时间
};