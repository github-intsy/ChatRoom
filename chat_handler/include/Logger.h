#pragma
#include <string>
#include <queue>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <fstream>
#include <atomic>
#include <iostream>
#define LOG_DEBUG(msg) Logger::getInstance().log(Logger::DEBUG, msg)
#define LOG_INFO(msg) Logger::getInstance().log(Logger::INFO, msg)
#define LOG_WARN(msg) Logger::getInstance().log(Logger::WARN, msg)
#define LOG_ERROR(msg) Logger::getInstance().log(Logger::ERROR, msg)

class Logger
{
public:
    enum LogLevel
    {
        DEBUG,
        INFO,
        WARN,
        ERROR
    };
    static Logger &getInstance();

    void start(const std::string &filename = "");
    void stop();

    void log(LogLevel level, const std::string &msg);

private:
    Logger();
    ~Logger();

    void writeThreadFunc();

private:
    std::queue<std::string> _logQueue;
    std::mutex _mutex;
    std::condition_variable _cv;

    std::thread _writeThread;
    std::ofstream _ofs;
    std::ostream *_os;
    std::atomic<bool> _running;
};