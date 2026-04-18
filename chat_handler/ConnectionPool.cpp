#include "ConnectionPool.h"
#include "MysqlConnection.h"
#include "Logger.h"
#include <cstdio>
#include <thread>
#include <functional>
#include <cstdlib>
#include <cstring>
// 连接池的构造
ConnectionPool::ConnectionPool()
{
    // 配置项加载失败就退出
    if (!loadConfigFile())
        return;

    for (int i = 0; i < _initSize; ++i)
    {
        MysqlConnection *p = new MysqlConnection();
        p->connect(_ip, _port, _username, _password, _dbname);
        p->refreshAliveTime(); // 刷新一下开始空闲的起始时间
        _connectionQue.emplace(p);
        _connectionCnt++; // 将连接加入连接池
    }

    // 创建一个新线程，作为连接的生产者
    std::thread produce(std::bind(&ConnectionPool::produceConnectionTask, this));
    produce.detach(); // 分离线程，自主回收资源
    // 启动一个新的定时线程，扫描超过maxIdleTime时间的空闲连接，进行连接的回收
    std::thread scanner(std::bind(&ConnectionPool::scannerConnectionTask, this));
    scanner.detach();
}

ConnectionPool *ConnectionPool::getConnectionPool()
{
    static ConnectionPool pool;
    return &pool;
}

bool ConnectionPool::loadConfigFile()
{
    FILE *pf = fopen("/home/gsy/OnlineChatRoom/mysql.ini", "r");
    if (pf == nullptr)
    {
        LOG_WARN("mysql.ini file is not exist!");
        return false;
    }
    while (!feof(pf))
    {
        char line[1024] = {0};
        fgets(line, 1024, pf);
        std::string str = line;
        int idx = str.find('=', 0);
        if (idx == -1) // 无效的配置项
            continue;
        // password=123456\n
        int endidx = str.find('\n', idx);
        std::string key = str.substr(0, idx);
        std::string value = str.substr(idx + 1, endidx - idx - 1);
        if (key == "ip")
            _ip = value;
        else if (key == "port")
            _port = atoi(value.c_str());
        else if (key == "username")
            _username = value;
        else if (key == "password")
            _password = value;
        else if (key == "dbname")
            _dbname = value;
        else if (key == "initSize")
            _initSize = atoi(value.c_str());
        else if (key == "maxSize")
            _maxSize = atoi(value.c_str());
        else if (key == "maxIdleTime")
            _maxIdleTime = atoi(value.c_str());
        else if (key == "connectionTimeOut")
            _connectionTimeout = atoi(value.c_str());
    }
    LOG_INFO("Mysql config success");
    fclose(pf);
    return true;
}

void ConnectionPool::produceConnectionTask()
{
    while (true)
    {
        std::unique_lock<std::mutex> lock(_queueMutex);
        while (!_connectionQue.empty())
            cv.wait(lock); // 队列不为空，线程进入等待模式

        // 连接数量没有达到上线，继续生产新的连接
        if (_connectionCnt < _maxSize)
        {
            MysqlConnection *p = new MysqlConnection();
            p->connect(_ip, _port, _username, _password, _dbname);
            p->refreshAliveTime(); // 刷新起始开始空闲时间
            _connectionQue.emplace(p);
            _connectionCnt++;
        }
        cv.notify_all(); // 通知消费者消费
    }
}

void ConnectionPool::scannerConnectionTask()
{
    while (true)
    {
        // 使用sleep模拟定时效果
        std::this_thread::sleep_for(std::chrono::seconds(_maxIdleTime));

        // 扫描整个队列，释放多余连接
        std::unique_lock<std::mutex> lock(_queueMutex);
        while (_connectionCnt > _initSize)
        {
            MysqlConnection *p = _connectionQue.front();
            if (p->getAliveTime() >= (_maxIdleTime * 1000))
            {
                _connectionQue.pop();
                _connectionCnt--;
                delete p;
            }
            else
            {
                break; // 队头都没有超时，其他肯定没有
            }
        }
    }
}

// 从连接池获得一个可用的空闲连接
std::shared_ptr<MysqlConnection> ConnectionPool::getConnection()
{
    std::unique_lock<std::mutex> lock(_queueMutex);
    while (_connectionQue.empty())
    {
        if (std::cv_status::timeout == cv.wait_for(lock, std::chrono::milliseconds(_connectionTimeout)))
        {
            if (_connectionQue.empty())
            {
                LOG_WARN("The timeout occurred while trying to obtain an empty connection... The connection acquisition failed");
                return nullptr;
            }
        }
    }

    /*
    shared_ptr在析构时，会把MysqlConnection资源直接delete掉，相当于直接调用MysqlConnection的析构函数
    所以需要自定义shared_ptr的释放资源的方式，把用好的连接放回queue中
    */
    std::shared_ptr<MysqlConnection> sp(_connectionQue.front(), [&](MysqlConnection *mpcon)
                                        {
        std::unique_lock<std::mutex> lock(_queueMutex);
        mpcon->refreshAliveTime();
        _connectionQue.push(mpcon); });
    _connectionQue.pop();
    cv.notify_all();
    return sp;
}