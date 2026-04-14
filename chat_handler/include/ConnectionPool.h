#pragma once
#include "MysqlConnection.h"
#include <string>
#include <queue>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
class MysqlConnection;

class ConnectionPool
{
public:
    // 获取连接池对象实例
    static ConnectionPool *getConnectionPool();
    std::shared_ptr<MysqlConnection> getConnection();

    ConnectionPool(ConnectionPool &) = delete;
    ConnectionPool operator=(ConnectionPool &) = delete;

private:
    // 加载配置文件，为mysql.ini
    bool loadConfigFile();
    // 生产新链接的线程函数
    void produceConnectionTask();
    void scannerConnectionTask();
    ConnectionPool();
    std::string _ip;        // mysql的IP地址
    uint16_t _port;         // mysql的端口号
    std::string _username;  // mysql登录用户名
    std::string _password;  // mysql登录密码
    std::string _dbname;    // 连接的数据库名称
    int _initSize;          // 连接池的初始连接量
    int _maxSize;           // 连接池的最大连接量
    int _maxIdleTime;       // 连接池的最大空闲时间
    int _connectionTimeout; // 连接池获取连接的超时时间

    std::queue<MysqlConnection *> _connectionQue; // 存储mysql连接
    std::mutex _queueMutex;                       // 维护连接队列的线程安全互斥锁
    std::atomic_int _connectionCnt;               // 原子修改操作，记录连接创建的mysqlConnection连接总数
    std::condition_variable cv;                   // 设置条件变量，用于连接生产线程和连接消费线程的通信
};